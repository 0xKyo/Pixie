#include "core/Image.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>

namespace pixie {

static_assert(sizeof(RGB8) == 4, "RGB8 must be exactly 4 bytes to alias a tightly packed RGBA8 buffer");

void Image::Resize(int width, int height) {
    width_ = width;
    height_ = height;
    pixels_.assign(static_cast<size_t>(width) * static_cast<size_t>(height), RGB8{});
}

void Image::Clear() {
    width_ = 0;
    height_ = 0;
    pixels_.clear();
}

bool Image::LoadFromFile(const std::filesystem::path& path, std::string* error) {
    // Read the file ourselves (rather than letting stb_image fopen() the path)
    // so that non-ASCII paths work correctly on Windows.
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error) *error = "Could not open file: " + path.string();
        return false;
    }
    std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (buffer.empty()) {
        if (error) *error = "Empty or unreadable file: " + path.string();
        return false;
    }

    int w = 0, h = 0, channelsInFile = 0;
    stbi_uc* data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &w, &h, &channelsInFile, 4);
    if (!data) {
        if (error) *error = stbi_failure_reason() ? stbi_failure_reason() : "Unknown image decoding error";
        return false;
    }

    width_ = w;
    height_ = h;
    pixels_.resize(static_cast<size_t>(w) * static_cast<size_t>(h));
    std::memcpy(pixels_.data(), data, pixels_.size() * sizeof(RGB8));
    stbi_image_free(data);
    return true;
}

bool Image::SaveToPng(const std::filesystem::path& path, std::string* error) const {
    if (Empty()) {
        if (error) *error = "Cannot save an empty image";
        return false;
    }

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        if (error) *error = "Could not open file for writing: " + path.string();
        return false;
    }

    struct WriteCtx {
        std::ofstream* out;
        bool failed = false;
    } ctx{&file};

    auto writeFunc = [](void* context, void* data, int size) {
        auto* c = static_cast<WriteCtx*>(context);
        if (!c->out->write(static_cast<const char*>(data), size)) {
            c->failed = true;
        }
    };

    const int stride = width_ * static_cast<int>(sizeof(RGB8));
    const int ok = stbi_write_png_to_func(writeFunc, &ctx, width_, height_, 4, pixels_.data(), stride);
    if (!ok || ctx.failed || !file) {
        if (error) *error = "Failed to encode/write PNG: " + path.string();
        return false;
    }
    return true;
}

namespace {

void PutByte(std::vector<unsigned char>& b, unsigned v) { b.push_back(static_cast<unsigned char>(v & 0xFF)); }
void PutWord(std::vector<unsigned char>& b, unsigned v) { PutByte(b, v); PutByte(b, v >> 8); }
void PutDword(std::vector<unsigned char>& b, uint32_t v) { PutWord(b, v & 0xFFFF); PutWord(b, v >> 16); }
void PutZeros(std::vector<unsigned char>& b, size_t n) { b.insert(b.end(), n, 0); }

} // namespace

bool Image::SaveToAseprite(const std::filesystem::path& path, std::string* error) const {
    if (Empty()) {
        if (error) *error = "Cannot save an empty image";
        return false;
    }
    if (width_ > 0xFFFF || height_ > 0xFFFF) {
        if (error) *error = "Image too large for the Aseprite format";
        return false;
    }

    // Layer chunk (0x2004): one visible, editable, normal-blend layer.
    std::vector<unsigned char> layer;
    PutWord(layer, 3);   // flags: visible | editable
    PutWord(layer, 0);   // layer type: normal
    PutWord(layer, 0);   // child level
    PutWord(layer, 0);   // default width (ignored)
    PutWord(layer, 0);   // default height (ignored)
    PutWord(layer, 0);   // blend mode: normal
    PutByte(layer, 255); // opacity
    PutZeros(layer, 3);
    const char layerName[] = "Layer 1";
    PutWord(layer, sizeof(layerName) - 1);
    layer.insert(layer.end(), layerName, layerName + sizeof(layerName) - 1);

    // Cel chunk (0x2005): raw (uncompressed) RGBA image at (0, 0).
    std::vector<unsigned char> cel;
    PutWord(cel, 0);     // layer index
    PutWord(cel, 0);     // x
    PutWord(cel, 0);     // y
    PutByte(cel, 255);   // opacity
    PutWord(cel, 0);     // cel type: raw image data
    PutWord(cel, 0);     // z-index
    PutZeros(cel, 5);
    PutWord(cel, static_cast<unsigned>(width_));
    PutWord(cel, static_cast<unsigned>(height_));
    const auto* raw = reinterpret_cast<const unsigned char*>(pixels_.data());
    cel.insert(cel.end(), raw, raw + pixels_.size() * sizeof(RGB8));

    std::vector<unsigned char> frame;
    PutDword(frame, 0); // frame size, patched below
    PutWord(frame, 0xF1FA);
    PutWord(frame, 2);   // old chunk count
    PutWord(frame, 100); // duration (ms)
    PutZeros(frame, 2);
    PutDword(frame, 2);  // new chunk count
    PutDword(frame, static_cast<uint32_t>(layer.size() + 6));
    PutWord(frame, 0x2004);
    frame.insert(frame.end(), layer.begin(), layer.end());
    PutDword(frame, static_cast<uint32_t>(cel.size() + 6));
    PutWord(frame, 0x2005);
    frame.insert(frame.end(), cel.begin(), cel.end());
    const uint32_t frameSize = static_cast<uint32_t>(frame.size());
    for (int i = 0; i < 4; ++i) frame[i] = static_cast<unsigned char>((frameSize >> (8 * i)) & 0xFF);

    std::vector<unsigned char> out;
    PutDword(out, static_cast<uint32_t>(128 + frame.size()));
    PutWord(out, 0xA5E0);
    PutWord(out, 1); // frames
    PutWord(out, static_cast<unsigned>(width_));
    PutWord(out, static_cast<unsigned>(height_));
    PutWord(out, 32); // RGBA
    PutDword(out, 1); // flags: layer opacity valid
    PutWord(out, 100);
    PutZeros(out, 8);
    PutByte(out, 0);  // transparent palette index
    PutZeros(out, 3);
    PutWord(out, 0);  // number of colors (0 = 256 for old sprites)
    PutByte(out, 1);  // pixel width
    PutByte(out, 1);  // pixel height
    PutZeros(out, 8); // grid x, y, w, h
    PutZeros(out, 84);
    out.insert(out.end(), frame.begin(), frame.end());

    std::ofstream file(path, std::ios::binary);
    if (!file || !file.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()))) {
        if (error) *error = "Could not write file: " + path.string();
        return false;
    }
    return true;
}

} // namespace pixie
