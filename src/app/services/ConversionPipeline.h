#pragma once

#include "core/color/Palette.h"
#include "core/graphics/Crop.h"
#include "core/graphics/Image.h"
#include "core/graphics/Mask.h"

namespace pixie::services {

// The converter's tool toggles that apply the same way to every image.
struct ConversionSettings {
    bool removeTransparency = false;
    bool resize = false;
    int targetWidth = 0;
    int targetHeight = 0;
};

// Crop -> remove transparency -> clear background -> remap -> resize -> binarize alpha (null/empty args skip a step).
graphics::Image ConvertImage(const graphics::Image& source, const ConversionSettings& settings,
                             const graphics::Mask* backgroundMask, const graphics::CropRect& crop,
                             const color::Palette* palette);

} // namespace pixie::services
