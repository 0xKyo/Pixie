#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "core/graphics/Image.h"

namespace pixie::formats {

// Every frame of an Aseprite file, flattened, with per-frame durations and animation tags.
struct AsepriteAnimation {
    struct Tag {
        std::string name;
        int firstFrame = 0; // inclusive
        int lastFrame = 0;  // inclusive
    };
    std::vector<graphics::Image> frames;
    std::vector<int> frameDurationsMs; // one per frame
    std::vector<Tag> tags;
};

// True for .aseprite and .ase (any letter case).
[[nodiscard]] bool IsAsepritePath(const std::filesystem::path& path);

// Decodes all frames: visible layers are composited with normal blending and layer/cel opacity.
bool DecodeAseprite(const unsigned char* data, size_t size, AsepriteAnimation& animation, std::string* error);

// Decodes only the first frame (what a static preview or the converter needs).
bool DecodeAsepriteFirstFrame(const unsigned char* data, size_t size, graphics::Image& image, std::string* error);

// Encodes one layer with one cel per frame, all frames the same size and the same duration.
bool EncodeAsepriteAnimation(const std::vector<graphics::Image>& frames, int frameDurationMs, std::vector<unsigned char>& bytes,
                             std::string* error);

// Encodes a single-frame, single-layer RGBA Aseprite file.
bool EncodeAseprite(const graphics::Image& image, std::vector<unsigned char>& bytes, std::string* error);

} // namespace pixie::formats
