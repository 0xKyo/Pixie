#pragma once

#include "core/Image.h"
#include "core/Palette.h"

namespace pixie {

struct ConvertOptions {
    // 0 = use std::thread::hardware_concurrency().
    unsigned threadCount = 0;
};

// Replaces each pixel with its closest CIEDE2000 match in `palette` (pixels
// pass through unchanged if `palette` is empty). Alpha is preserved; `dst` is resized to match `src`.
void ConvertImage(const Image& src, const Palette& palette, Image& dst, const ConvertOptions& opts = {});

// Snaps every pixel's alpha to fully opaque (255) or fully transparent (0),
// rounding to whichever is closer.
void BinarizeAlpha(Image& image);

// Makes every pixel with any alpha (> 0) fully opaque; fully transparent pixels stay at 0.
void RemoveTransparency(Image& image);

} // namespace pixie
