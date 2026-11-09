#pragma once

#include "core/graphics/Image.h"

namespace pixie::graphics {

// Nearest-neighbor resize: every output pixel copies one input pixel, so no new colors appear.
void ResizeNearest(const Image& source, int width, int height, Image& result);

// Box-filtered copy whose longest side is at most `maxSide`; smaller images are returned as is.
Image MakeThumbnail(const Image& source, int maxSide);

} // namespace pixie::graphics
