#include "core/formats/Aseprite.h"

#include <stb_image.h> // stbi_zlib_decode_malloc; the implementation is compiled in ImageFile.cpp

#include <algorithm>
#include <cstdint>

// Format reference: https://github.com/aseprite/aseprite/blob/main/docs/ase-file-specs.md
namespace pixie::formats {

namespace {

constexpr unsigned kFileMagic = 0xA5E0;
constexpr unsigned kFrameMagic = 0xF1FA;
constexpr size_t kHeaderSize = 128;

constexpr unsigned kLayerChunk = 0x2004;
constexpr unsigned kCelChunk = 0x2005;
constexpr unsigned kTagsChunk = 0x2018;
constexpr unsigned kPaletteChunk = 0x2019;

constexpr unsigned kRawCel = 0;
constexpr unsigned kLinkedCel = 1;
constexpr unsigned kCompressedCel = 2;

// Little-endian reader; reading past the end yields zeros instead of crashing.
class ByteReader {
public:
    ByteReader(const unsigned char* data, size_t size) : data_(data), size_(size) {}

    [[nodiscard]] size_t Offset() const { return offset_; }
    [[nodiscard]] bool HasBytes(size_t count) const { return offset_ + count <= size_; }
    [[nodiscard]] const unsigned char* Current() const { return data_ + offset_; }
    void Seek(size_t offset) { offset_ = std::min(size_, offset); }
    void Skip(size_t count) { Seek(offset_ + count); }

    unsigned Byte() { return HasBytes(1) ? data_[offset_++] : 0; }
    unsigned Word() {
        const unsigned low = Byte();
        return low | (Byte() << 8);
    }
    int Short() { return static_cast<int16_t>(Word()); }
    uint32_t Dword() {
        const uint32_t low = Word();
        return low | (static_cast<uint32_t>(Word()) << 16);
    }
    std::string String() {
        const unsigned length = Word();
        std::string text;
        for (unsigned i = 0; i < length && HasBytes(1); ++i) text.push_back(static_cast<char>(Byte()));
        return text;
    }

private:
    const unsigned char* data_;
    size_t size_;
    size_t offset_ = 0;
};

struct Header {
    unsigned frameCount = 0;
    unsigned width = 0;
    unsigned height = 0;
    unsigned bitsPerPixel = 0; // 32 = RGBA, 16 = grayscale + alpha, 8 = indexed
    unsigned defaultFrameMs = 100;
    unsigned transparentIndex = 0;
};

struct Layer {
    bool visible = true; // already includes the visibility of parent groups
    bool isImageLayer = true;
    unsigned opacity = 255;
};

// A layer's pixels in one frame, placed at (x, y) on the canvas.
struct Cel {
    unsigned layerIndex = 0;
    int zIndex = 0;
    int x = 0;
    int y = 0;
    unsigned opacity = 255;
    int width = 0;
    int height = 0;
    std::vector<color::Rgba> pixels;
};

// Everything that persists from one frame to the next while decoding.
struct DecodeState {
    Header header;
    std::vector<Layer> layers;
    std::vector<bool> visibleAtLevel; // visibility of the latest layer seen at each nesting depth
    std::vector<color::Rgba> palette = std::vector<color::Rgba>(256, color::Rgba{0, 0, 0, 255});
    std::vector<std::vector<Cel>> celsByFrame; // kept so linked cels can copy earlier frames
};

uint8_t MultiplyAlpha(unsigned first, unsigned second) { return static_cast<uint8_t>((first * second + 127) / 255); }

bool ReadHeader(ByteReader& reader, Header& header, std::string* error) {
    auto fail = [&](const char* message) {
        if (error) *error = message;
        return false;
    };
    if (!reader.HasBytes(kHeaderSize)) return fail("Not an Aseprite file (too small)");
    reader.Skip(4); // file size
    if (reader.Word() != kFileMagic) return fail("Not an Aseprite file (bad magic number)");
    header.frameCount = reader.Word();
    header.width = reader.Word();
    header.height = reader.Word();
    header.bitsPerPixel = reader.Word();
    reader.Skip(4); // flags
    const unsigned speed = reader.Word();
    header.defaultFrameMs = speed ? speed : 100;
    reader.Seek(28);
    header.transparentIndex = reader.Byte();
    reader.Seek(kHeaderSize);

    if (header.frameCount == 0 || header.width == 0 || header.height == 0) return fail("Empty Aseprite file");
    if (header.bitsPerPixel != 32 && header.bitsPerPixel != 16 && header.bitsPerPixel != 8) {
        return fail("Unsupported Aseprite color depth");
    }
    return true;
}

void ReadLayerChunk(ByteReader& reader, DecodeState& state) {
    const unsigned flags = reader.Word();
    const unsigned layerType = reader.Word();
    const unsigned nestingLevel = reader.Word();
    reader.Skip(4); // default width/height (unused)
    reader.Skip(2); // blend mode (only normal is supported)
    const unsigned opacity = reader.Byte();

    // A layer is visible only if it and every parent group are visible.
    const bool parentVisible = nestingLevel == 0 ||
                               (nestingLevel - 1 < state.visibleAtLevel.size() && state.visibleAtLevel[nestingLevel - 1]);
    Layer layer;
    layer.visible = (flags & 1) && parentVisible;
    layer.isImageLayer = layerType == 0;
    layer.opacity = opacity;

    if (state.visibleAtLevel.size() <= nestingLevel) state.visibleAtLevel.resize(nestingLevel + 1, false);
    state.visibleAtLevel[nestingLevel] = layer.visible;
    state.layers.push_back(layer);
}

// Converts raw cel bytes (in the file's color depth) into RGBA pixels.
std::vector<color::Rgba> DecodePixels(const unsigned char* raw, size_t pixelCount, const DecodeState& state) {
    const unsigned bitsPerPixel = state.header.bitsPerPixel;
    const size_t bytesPerPixel = bitsPerPixel / 8;
    std::vector<color::Rgba> pixels(pixelCount);
    for (size_t i = 0; i < pixelCount; ++i) {
        const unsigned char* pixel = raw + i * bytesPerPixel;
        if (bitsPerPixel == 32) {
            pixels[i] = color::Rgba{pixel[0], pixel[1], pixel[2], pixel[3]};
        } else if (bitsPerPixel == 16) {
            pixels[i] = color::Rgba{pixel[0], pixel[0], pixel[0], pixel[1]};
        } else if (pixel[0] == state.header.transparentIndex) {
            pixels[i] = color::kTransparent;
        } else {
            pixels[i] = state.palette[pixel[0]];
        }
    }
    return pixels;
}

void ReadCelChunk(ByteReader& reader, size_t chunkEnd, DecodeState& state, std::vector<Cel>& frameCels) {
    Cel cel;
    cel.layerIndex = reader.Word();
    cel.x = reader.Short();
    cel.y = reader.Short();
    cel.opacity = reader.Byte();
    const unsigned celType = reader.Word();
    cel.zIndex = reader.Short();
    reader.Skip(5);

    // A linked cel reuses the same layer's cel from an earlier frame.
    if (celType == kLinkedCel) {
        const unsigned linkedFrame = reader.Word();
        if (linkedFrame >= state.celsByFrame.size()) return;
        for (const Cel& earlier : state.celsByFrame[linkedFrame]) {
            if (earlier.layerIndex != cel.layerIndex) continue;
            Cel copy = earlier;
            copy.opacity = cel.opacity;
            copy.zIndex = cel.zIndex;
            frameCels.push_back(std::move(copy));
            return;
        }
        return;
    }
    if (celType != kRawCel && celType != kCompressedCel) return; // tilemaps are not supported

    cel.width = static_cast<int>(reader.Word());
    cel.height = static_cast<int>(reader.Word());
    const size_t pixelCount = static_cast<size_t>(cel.width) * cel.height;
    const size_t expectedBytes = pixelCount * (state.header.bitsPerPixel / 8);

    std::vector<unsigned char> raw;
    if (celType == kRawCel) {
        if (reader.Offset() + expectedBytes <= chunkEnd) raw.assign(reader.Current(), reader.Current() + expectedBytes);
    } else if (reader.Offset() < chunkEnd) {
        int inflatedSize = 0;
        char* inflated = stbi_zlib_decode_malloc(reinterpret_cast<const char*>(reader.Current()),
                                                  static_cast<int>(chunkEnd - reader.Offset()), &inflatedSize);
        if (inflated) {
            if (static_cast<size_t>(inflatedSize) >= expectedBytes) {
                const auto* bytes = reinterpret_cast<const unsigned char*>(inflated);
                raw.assign(bytes, bytes + expectedBytes);
            }
            stbi_image_free(inflated);
        }
    }
    if (raw.size() != expectedBytes) return; // truncated or corrupt: skip the cel

    cel.pixels = DecodePixels(raw.data(), pixelCount, state);
    frameCels.push_back(std::move(cel));
}

void ReadTagsChunk(ByteReader& reader, size_t chunkEnd, AsepriteAnimation& animation) {
    const unsigned tagCount = reader.Word();
    reader.Skip(8);
    for (unsigned i = 0; i < tagCount && reader.Offset() + 19 <= chunkEnd; ++i) {
        AsepriteAnimation::Tag tag;
        tag.firstFrame = static_cast<int>(reader.Word());
        tag.lastFrame = static_cast<int>(reader.Word());
        reader.Skip(1 + 2 + 6 + 3 + 1); // direction, repeat count, reserved, color, extra byte
        tag.name = reader.String();
        animation.tags.push_back(std::move(tag));
    }
}

void ReadPaletteChunk(ByteReader& reader, size_t chunkEnd, DecodeState& state) {
    const uint32_t entryCount = reader.Dword();
    const uint32_t firstIndex = reader.Dword();
    const uint32_t lastIndex = reader.Dword();
    reader.Skip(8);
    if (entryCount == 0 || firstIndex > lastIndex) return;
    if (state.palette.size() < entryCount) state.palette.resize(entryCount, color::Rgba{0, 0, 0, 255});

    for (uint32_t index = firstIndex; index <= lastIndex && index < state.palette.size() && reader.Offset() + 6 <= chunkEnd; ++index) {
        const unsigned entryFlags = reader.Word();
        const auto red = static_cast<uint8_t>(reader.Byte());
        const auto green = static_cast<uint8_t>(reader.Byte());
        const auto blue = static_cast<uint8_t>(reader.Byte());
        const auto alpha = static_cast<uint8_t>(reader.Byte());
        state.palette[index] = color::Rgba{red, green, blue, alpha};
        if (entryFlags & 1) reader.Skip(reader.Word()); // the entry has a name
    }
}

// Composites one frame's cels (bottom layer first) onto a transparent canvas.
graphics::Image FlattenCels(std::vector<Cel>& cels, const DecodeState& state) {
    const int canvasWidth = static_cast<int>(state.header.width);
    const int canvasHeight = static_cast<int>(state.header.height);
    graphics::Image canvas;
    canvas.Resize(canvasWidth, canvasHeight, color::kTransparent);

    std::stable_sort(cels.begin(), cels.end(), [](const Cel& first, const Cel& second) {
        return static_cast<int>(first.layerIndex) + first.zIndex < static_cast<int>(second.layerIndex) + second.zIndex;
    });

    for (const Cel& cel : cels) {
        if (cel.layerIndex >= state.layers.size()) continue;
        const Layer& layer = state.layers[cel.layerIndex];
        if (!layer.visible || !layer.isImageLayer) continue;
        const unsigned celOpacity = MultiplyAlpha(layer.opacity, cel.opacity);

        for (int celY = 0; celY < cel.height; ++celY) {
            const int canvasY = cel.y + celY;
            if (canvasY < 0 || canvasY >= canvasHeight) continue;
            for (int celX = 0; celX < cel.width; ++celX) {
                const int canvasX = cel.x + celX;
                if (canvasX < 0 || canvasX >= canvasWidth) continue;

                const color::Rgba top = cel.pixels[static_cast<size_t>(celY) * cel.width + celX];
                const unsigned topAlpha = MultiplyAlpha(top.a, celOpacity);
                if (topAlpha == 0) continue;
                const color::Rgba bottom = canvas.GetPixel(canvasX, canvasY);

                // Standard "over" blend on straight (non-premultiplied) alpha.
                const unsigned resultAlpha = topAlpha + MultiplyAlpha(bottom.a, 255 - topAlpha);
                auto blend = [&](unsigned topChannel, unsigned bottomChannel) {
                    const unsigned weighted = topChannel * topAlpha * 255 + bottomChannel * bottom.a * (255 - topAlpha);
                    return static_cast<uint8_t>(std::min<unsigned>(255, weighted / (resultAlpha * 255)));
                };
                canvas.SetPixel(canvasX, canvasY, color::Rgba{blend(top.r, bottom.r), blend(top.g, bottom.g),
                                                              blend(top.b, bottom.b), static_cast<uint8_t>(resultAlpha)});
            }
        }
    }
    return canvas;
}

bool Decode(const unsigned char* data, size_t size, size_t maxFrames, AsepriteAnimation& animation, std::string* error) {
    ByteReader reader(data, size);
    DecodeState state;
    if (!ReadHeader(reader, state.header, error)) return false;

    animation = AsepriteAnimation{};
    const size_t framesToRead = std::min<size_t>(state.header.frameCount, maxFrames);
    for (size_t frameIndex = 0; frameIndex < framesToRead; ++frameIndex) {
        if (!reader.HasBytes(16)) {
            if (error) *error = "Truncated Aseprite file";
            return false;
        }
        const size_t frameStart = reader.Offset();
        const uint32_t frameBytes = reader.Dword();
        if (reader.Word() != kFrameMagic) {
            if (error) *error = "Corrupt Aseprite frame header";
            return false;
        }
        const unsigned oldChunkCount = reader.Word();
        const unsigned frameDurationMs = reader.Word();
        reader.Skip(2);
        const uint32_t newChunkCount = reader.Dword();
        const uint32_t chunkCount = newChunkCount ? newChunkCount : oldChunkCount;
        const size_t frameEnd = std::min(size, frameStart + frameBytes);

        std::vector<Cel> frameCels;
        for (uint32_t chunk = 0; chunk < chunkCount && reader.Offset() + 6 <= frameEnd; ++chunk) {
            const size_t chunkStart = reader.Offset();
            const uint32_t chunkSize = reader.Dword();
            const unsigned chunkType = reader.Word();
            if (chunkSize < 6) break;
            const size_t chunkEnd = std::min(frameEnd, chunkStart + chunkSize);

            switch (chunkType) {
            case kLayerChunk: ReadLayerChunk(reader, state); break;
            case kCelChunk: ReadCelChunk(reader, chunkEnd, state, frameCels); break;
            case kTagsChunk: ReadTagsChunk(reader, chunkEnd, animation); break;
            case kPaletteChunk: ReadPaletteChunk(reader, chunkEnd, state); break;
            default: break; // other chunks (user data, slices, ...) are irrelevant here
            }
            reader.Seek(chunkEnd);
        }
        reader.Seek(frameEnd);

        animation.frames.push_back(FlattenCels(frameCels, state));
        animation.frameDurationsMs.push_back(static_cast<int>(frameDurationMs ? frameDurationMs : state.header.defaultFrameMs));
        state.celsByFrame.push_back(std::move(frameCels));
    }
    return true;
}

// Little-endian writers for the encoder.
void PutByte(std::vector<unsigned char>& out, unsigned value) { out.push_back(static_cast<unsigned char>(value & 0xFF)); }
void PutWord(std::vector<unsigned char>& out, unsigned value) {
    PutByte(out, value);
    PutByte(out, value >> 8);
}
void PutDword(std::vector<unsigned char>& out, uint32_t value) {
    PutWord(out, value & 0xFFFF);
    PutWord(out, value >> 16);
}
void PutZeros(std::vector<unsigned char>& out, size_t count) { out.insert(out.end(), count, 0); }

} // namespace

bool IsAsepritePath(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; });
    return extension == ".aseprite" || extension == ".ase";
}

bool DecodeAseprite(const unsigned char* data, size_t size, AsepriteAnimation& animation, std::string* error) {
    return Decode(data, size, static_cast<size_t>(-1), animation, error);
}

bool DecodeAsepriteFirstFrame(const unsigned char* data, size_t size, graphics::Image& image, std::string* error) {
    AsepriteAnimation animation;
    if (!Decode(data, size, 1, animation, error) || animation.frames.empty()) return false;
    image = std::move(animation.frames.front());
    return true;
}

namespace {

std::vector<unsigned char> BuildLayerChunk() {
    std::vector<unsigned char> layer;
    PutWord(layer, 3);   // flags: visible | editable
    PutWord(layer, 0);   // layer type: image
    PutWord(layer, 0);   // nesting level
    PutWord(layer, 0);   // default width (ignored)
    PutWord(layer, 0);   // default height (ignored)
    PutWord(layer, 0);   // blend mode: normal
    PutByte(layer, 255); // opacity
    PutZeros(layer, 3);
    const std::string layerName = "Layer 1";
    PutWord(layer, static_cast<unsigned>(layerName.size()));
    layer.insert(layer.end(), layerName.begin(), layerName.end());
    return layer;
}

// The whole image, uncompressed, at (0, 0) on layer 0.
std::vector<unsigned char> BuildCelChunk(const graphics::Image& image) {
    const auto width = static_cast<unsigned>(image.Width());
    const auto height = static_cast<unsigned>(image.Height());
    std::vector<unsigned char> cel;
    PutWord(cel, 0);   // layer index
    PutWord(cel, 0);   // x
    PutWord(cel, 0);   // y
    PutByte(cel, 255); // opacity
    PutWord(cel, kRawCel);
    PutWord(cel, 0);   // z-index
    PutZeros(cel, 5);
    PutWord(cel, width);
    PutWord(cel, height);
    const auto* pixelBytes = reinterpret_cast<const unsigned char*>(image.Pixels());
    cel.insert(cel.end(), pixelBytes, pixelBytes + static_cast<size_t>(width) * height * 4);
    return cel;
}

// The layer chunk is only written in the first frame.
std::vector<unsigned char> BuildFrame(const graphics::Image& image, bool includeLayer, int durationMs) {
    const uint32_t chunkCount = includeLayer ? 2 : 1;
    std::vector<unsigned char> frame;
    PutDword(frame, 0); // frame size, patched below
    PutWord(frame, kFrameMagic);
    PutWord(frame, chunkCount); // old chunk count
    PutWord(frame, static_cast<unsigned>(durationMs));
    PutZeros(frame, 2);
    PutDword(frame, chunkCount); // new chunk count
    if (includeLayer) {
        const std::vector<unsigned char> layer = BuildLayerChunk();
        PutDword(frame, static_cast<uint32_t>(layer.size() + 6));
        PutWord(frame, kLayerChunk);
        frame.insert(frame.end(), layer.begin(), layer.end());
    }
    const std::vector<unsigned char> cel = BuildCelChunk(image);
    PutDword(frame, static_cast<uint32_t>(cel.size() + 6));
    PutWord(frame, kCelChunk);
    frame.insert(frame.end(), cel.begin(), cel.end());
    const auto frameSize = static_cast<uint32_t>(frame.size());
    for (int i = 0; i < 4; ++i) frame[i] = static_cast<unsigned char>((frameSize >> (8 * i)) & 0xFF);
    return frame;
}

} // namespace

bool EncodeAsepriteAnimation(const std::vector<graphics::Image>& frames, int frameDurationMs, std::vector<unsigned char>& bytes,
                             std::string* error) {
    if (frames.empty() || frames.front().Empty()) {
        if (error) *error = "Cannot save an empty image";
        return false;
    }
    const int imageWidth = frames.front().Width();
    const int imageHeight = frames.front().Height();
    if (imageWidth > 0xFFFF || imageHeight > 0xFFFF) {
        if (error) *error = "Image too large for the Aseprite format";
        return false;
    }
    if (frames.size() > 0xFFFF) {
        if (error) *error = "Too many frames for the Aseprite format";
        return false;
    }
    for (size_t index = 1; index < frames.size(); ++index) {
        if (frames[index].Width() != imageWidth || frames[index].Height() != imageHeight) {
            if (error) *error = "Frame " + std::to_string(index + 1) + " is not the same size as the first frame";
            return false;
        }
    }
    const int durationMs = std::clamp(frameDurationMs, 1, 0xFFFF);

    std::vector<unsigned char> frameBytes;
    for (size_t index = 0; index < frames.size(); ++index) {
        const std::vector<unsigned char> frame = BuildFrame(frames[index], index == 0, durationMs);
        frameBytes.insert(frameBytes.end(), frame.begin(), frame.end());
    }

    bytes.clear();
    PutDword(bytes, static_cast<uint32_t>(kHeaderSize + frameBytes.size()));
    PutWord(bytes, kFileMagic);
    PutWord(bytes, static_cast<unsigned>(frames.size()));
    PutWord(bytes, static_cast<unsigned>(imageWidth));
    PutWord(bytes, static_cast<unsigned>(imageHeight));
    PutWord(bytes, 32); // color depth: RGBA
    PutDword(bytes, 1); // flags: layer opacity is valid
    PutWord(bytes, static_cast<unsigned>(durationMs));
    PutZeros(bytes, 8);
    PutByte(bytes, 0);  // transparent palette index
    PutZeros(bytes, 3);
    PutWord(bytes, 0);  // color count (0 means 256 for old files)
    PutByte(bytes, 1);  // pixel width
    PutByte(bytes, 1);  // pixel height
    PutZeros(bytes, 8); // grid x, y, width, height
    PutZeros(bytes, 84);
    bytes.insert(bytes.end(), frameBytes.begin(), frameBytes.end());
    return true;
}

bool EncodeAseprite(const graphics::Image& image, std::vector<unsigned char>& bytes, std::string* error) {
    return EncodeAsepriteAnimation({image}, 100, bytes, error);
}

} // namespace pixie::formats
