#pragma once

#include "core/Image.h"

namespace pixie {

// Resizes `src` to `dstWidth`x`dstHeight` via coverage-weighted box-filter
// resampling (each axis independent, so non-square aspect changes stretch).
// Alpha is premultiplied first so a transparent pixel's leftover RGB never
// bleeds into an opaque edge. Named for its main use (pixel-art downscale)
// but the filter is coverage-based, so it enlarges just as correctly.
void DownscaleImage(const Image& src, int dstWidth, int dstHeight, Image& dst);

} // namespace pixie
