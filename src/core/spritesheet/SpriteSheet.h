#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/graphics/Image.h"

namespace pixie::spritesheet {

// One Aseprite file to pack, and the asset name written to the layout.
struct Source {
    std::string assetName;
    std::filesystem::path asepritePath;
};

struct Options {
    int maxWidth = 1024; // rows wrap at this width (a wider frame widens the atlas)
    int padding = 0;     // empty pixels between frames
};

struct Result {
    graphics::Image atlas;
    std::string layoutJson; // {atlas_width, atlas_height, items: [{asset, frame_index, atlas_x, ...}]}
    int assetCount = 0;
    int frameCount = 0;
};

// Packs every frame of every source (full canvas, untrimmed) into one atlas plus its JSON layout.
bool Build(const std::vector<Source>& sources, const Options& options, Result& result, std::string* error);

} // namespace pixie::spritesheet
