#pragma once

#include "core/Image.h"

namespace pixie {

// Resizes `src` to `dstWidth`x`dstHeight` via nearest-neighbor sampling (each
// axis independent, so non-square aspect changes stretch). Every output pixel
// is a verbatim copy of one input pixel, so it never introduces a color that
// wasn't already in `src` -- critical for running after palette quantization.
void DownscaleImage(const Image& src, int dstWidth, int dstHeight, Image& dst);

} // namespace pixie
