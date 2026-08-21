#pragma once

#include "core/Image.h"
#include "core/Mask.h"

namespace pixie {

// An axis-aligned crop region in source-image pixel coordinates.
struct CropRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    [[nodiscard]] int Right() const { return x + width; }
    [[nodiscard]] int Bottom() const { return y + height; }
    [[nodiscard]] bool Empty() const { return width <= 0 || height <= 0; }
};

// Clamps `rect` to `src`'s bounds and copies that region into `dst`. Leaves
// `dst` untouched if the clamped rect is empty.
void CropImage(const Image& src, const CropRect& rect, Image& dst);

// Same semantics as CropImage, but for a Mask (used to keep a background
// mask aligned with a cropped image during conversion).
void CropMask(const Mask& src, const CropRect& rect, Mask& dst);

} // namespace pixie
