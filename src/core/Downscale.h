#pragma once

#include "core/Image.h"

namespace pixie {

// Returns the height that preserves src's aspect ratio for `targetWidth`,
// rounded to the nearest pixel (minimum 1). Returns 0 if srcWidth or
// targetWidth is <= 0.
int ComputeMatchingHeight(int srcWidth, int srcHeight, int targetWidth);

// Shrinks `src` to `dstWidth`x`dstHeight` via coverage-weighted box-filter
// averaging. Alpha is premultiplied first so a transparent pixel's leftover RGB never bleeds into an opaque edge.
void DownscaleImage(const Image& src, int dstWidth, int dstHeight, Image& dst);

} // namespace pixie
