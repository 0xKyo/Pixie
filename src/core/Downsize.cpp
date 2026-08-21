#include "core/Downsize.h"

#include <algorithm>
#include <cmath>

namespace pixie {

CropRect ComputeAspectFitRect(int srcW, int srcH, int targetW, int targetH) {
    if (srcW <= 0 || srcH <= 0 || targetW <= 0 || targetH <= 0) {
        return CropRect{};
    }
    const double srcAspect = static_cast<double>(srcW) / srcH;
    const double targetAspect = static_cast<double>(targetW) / targetH;

    int w, h;
    if (srcAspect > targetAspect) {
        h = srcH;
        w = std::max(1, static_cast<int>(std::lround(srcH * targetAspect)));
    } else {
        w = srcW;
        h = std::max(1, static_cast<int>(std::lround(srcW / targetAspect)));
    }
    w = std::min(w, srcW);
    h = std::min(h, srcH);
    return CropRect{(srcW - w) / 2, (srcH - h) / 2, w, h};
}

} // namespace pixie
