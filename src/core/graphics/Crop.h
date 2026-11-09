#pragma once

#include "core/graphics/Image.h"
#include "core/graphics/Mask.h"

namespace pixie::graphics {

// An axis-aligned rectangle in image pixel coordinates.
struct CropRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    [[nodiscard]] int Right() const { return x + width; }
    [[nodiscard]] int Bottom() const { return y + height; }
    [[nodiscard]] bool Empty() const { return width <= 0 || height <= 0; }
};

// Copies the part of `source` inside `rect` (clamped to the image) into `result`.
void CropImage(const Image& source, const CropRect& rect, Image& result);

// Same as CropImage, for keeping a mask aligned with a cropped image.
void CropMask(const Mask& source, const CropRect& rect, Mask& result);

// The largest rect with the target's aspect ratio that fits centered inside the image.
CropRect CenteredRectWithAspect(int imageWidth, int imageHeight, int targetWidth, int targetHeight);

} // namespace pixie::graphics
