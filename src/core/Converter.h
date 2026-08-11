#pragma once

#include "core/Image.h"
#include "core/Palette.h"

namespace pixie {

struct ConvertOptions {
    // 0 = use std::thread::hardware_concurrency().
    unsigned threadCount = 0;
};

// Converts `src` into `dst` by replacing every pixel with the closest color in
// `palette`, measured with the CIEDE2000 perceptual color difference. `dst` is
// resized to match `src`. Source alpha is preserved on the output pixels.
void ConvertImage(const Image& src, const Palette& palette, Image& dst, const ConvertOptions& opts = {});

} // namespace pixie
