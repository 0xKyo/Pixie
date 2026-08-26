#include "core/Downscale.h"

#include <algorithm>
#include <cstdint>

namespace pixie {

namespace {

// Maps output index `o` to the nearest input index along an axis of the given sizes.
int NearestIndex(int o, int srcSize, int dstSize) {
    const double srcPos = (static_cast<double>(o) + 0.5) * static_cast<double>(srcSize) / static_cast<double>(dstSize);
    return std::clamp(static_cast<int>(srcPos), 0, srcSize - 1);
}

} // namespace

void DownscaleImage(const Image& src, int dstWidth, int dstHeight, Image& dst) {
    if (src.Empty() || dstWidth <= 0 || dstHeight <= 0) {
        return;
    }
    const int srcW = src.Width();
    const int srcH = src.Height();

    dst.Resize(dstWidth, dstHeight);
    for (int y = 0; y < dstHeight; ++y) {
        const int sy = NearestIndex(y, srcH, dstHeight);
        for (int x = 0; x < dstWidth; ++x) {
            const int sx = NearestIndex(x, srcW, dstWidth);
            dst.SetPixel(x, y, src.GetPixel(sx, sy));
        }
    }
}

} // namespace pixie
