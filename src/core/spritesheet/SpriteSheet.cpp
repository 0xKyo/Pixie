#include "core/spritesheet/SpriteSheet.h"

#include <algorithm>

#include "core/formats/Aseprite.h"
#include "core/formats/BinaryFile.h"

namespace pixie::spritesheet {

namespace {

// One packed frame and where it landed in the atlas.
struct PlacedFrame {
    std::string assetName;
    int frameIndex = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int durationMs = 100;
    std::vector<std::string> tags;
    const graphics::Image* pixels = nullptr;
};

std::string EscapeJson(const std::string& text) {
    std::string escaped;
    for (const char c : text) {
        switch (c) {
        case '"': escaped += "\\\""; break;
        case '\\': escaped += "\\\\"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default: escaped += static_cast<unsigned char>(c) < 0x20 ? ' ' : c; // UTF-8 passes through
        }
    }
    return escaped;
}

// Tags of one frame: every tag whose range contains it; untagged animations get "default".
std::vector<std::string> TagsForFrame(const formats::AsepriteAnimation& animation, int frameIndex) {
    std::vector<std::string> tags;
    if (animation.frames.size() <= 1) return tags; // static assets have no animations
    for (const auto& tag : animation.tags) {
        if (frameIndex >= tag.firstFrame && frameIndex <= tag.lastFrame) tags.push_back(tag.name);
    }
    if (animation.tags.empty()) tags.push_back("default");
    return tags;
}

// Left-to-right rows ("shelves"); returns the atlas size.
std::pair<int, int> PackIntoRows(std::vector<PlacedFrame>& frames, const Options& options) {
    int widestFrame = 0;
    for (const PlacedFrame& frame : frames) widestFrame = std::max(widestFrame, frame.width);
    const int rowLimit = std::max(options.maxWidth, widestFrame);

    int cursorX = 0, cursorY = 0, rowHeight = 0, atlasWidth = 0;
    for (PlacedFrame& frame : frames) {
        if (cursorX > 0 && cursorX + frame.width > rowLimit) {
            cursorY += rowHeight + options.padding;
            cursorX = 0;
            rowHeight = 0;
        }
        frame.x = cursorX;
        frame.y = cursorY;
        cursorX += frame.width + options.padding;
        rowHeight = std::max(rowHeight, frame.height);
        atlasWidth = std::max(atlasWidth, frame.x + frame.width);
    }
    return {atlasWidth, cursorY + rowHeight};
}

std::string WriteLayoutJson(const std::vector<PlacedFrame>& frames, int atlasWidth, int atlasHeight) {
    std::string json = "{\n  \"atlas_width\": " + std::to_string(atlasWidth) + ",\n  \"atlas_height\": " +
                       std::to_string(atlasHeight) + ",\n  \"items\": [\n";
    for (size_t i = 0; i < frames.size(); ++i) {
        const PlacedFrame& frame = frames[i];
        json += "    {\"asset\": \"" + EscapeJson(frame.assetName) + "\", \"frame_index\": " + std::to_string(frame.frameIndex) +
                ", \"atlas_x\": " + std::to_string(frame.x) + ", \"atlas_y\": " + std::to_string(frame.y) +
                ", \"w\": " + std::to_string(frame.width) + ", \"h\": " + std::to_string(frame.height) +
                ", \"duration_ms\": " + std::to_string(frame.durationMs) + ", \"tags\": [";
        for (size_t t = 0; t < frame.tags.size(); ++t) {
            json += (t ? ", \"" : "\"") + EscapeJson(frame.tags[t]) + "\"";
        }
        json += "]}";
        json += i + 1 < frames.size() ? ",\n" : "\n";
    }
    json += "  ]\n}\n";
    return json;
}

} // namespace

bool Build(const std::vector<Source>& sources, const Options& options, Result& result, std::string* error) {
    auto fail = [&](const std::string& message) {
        if (error) *error = message;
        return false;
    };

    // Decode everything first; `frames` points into `animations`, which is never resized afterwards.
    std::vector<formats::AsepriteAnimation> animations(sources.size());
    std::vector<PlacedFrame> frames;
    for (size_t sourceIndex = 0; sourceIndex < sources.size(); ++sourceIndex) {
        const Source& source = sources[sourceIndex];
        std::vector<unsigned char> bytes;
        std::string decodeError;
        if (!formats::ReadWholeFile(source.asepritePath, bytes, &decodeError) ||
            !formats::DecodeAseprite(bytes.data(), bytes.size(), animations[sourceIndex], &decodeError)) {
            return fail(source.asepritePath.filename().string() + ": " + decodeError);
        }

        const formats::AsepriteAnimation& animation = animations[sourceIndex];
        for (int frameIndex = 0; frameIndex < static_cast<int>(animation.frames.size()); ++frameIndex) {
            PlacedFrame frame;
            frame.assetName = source.assetName;
            frame.frameIndex = frameIndex;
            frame.width = animation.frames[frameIndex].Width();
            frame.height = animation.frames[frameIndex].Height();
            frame.durationMs = animation.frameDurationsMs[frameIndex];
            frame.tags = TagsForFrame(animation, frameIndex);
            frame.pixels = &animation.frames[frameIndex];
            frames.push_back(std::move(frame));
        }
    }
    if (frames.empty()) return fail("Nothing to pack");

    const auto [atlasWidth, atlasHeight] = PackIntoRows(frames, options);
    result.atlas.Resize(atlasWidth, atlasHeight, color::kTransparent);
    for (const PlacedFrame& frame : frames) {
        for (int y = 0; y < frame.height; ++y) {
            for (int x = 0; x < frame.width; ++x) {
                result.atlas.SetPixel(frame.x + x, frame.y + y, frame.pixels->GetPixel(x, y));
            }
        }
    }

    result.layoutJson = WriteLayoutJson(frames, atlasWidth, atlasHeight);
    result.assetCount = static_cast<int>(sources.size());
    result.frameCount = static_cast<int>(frames.size());
    return true;
}

} // namespace pixie::spritesheet
