#include "core/Crop.h"

#include <algorithm>

namespace pixie {

namespace {

CropRect ClampRect(const CropRect& rect, int srcWidth, int srcHeight) {
    const int x0 = std::clamp(rect.x, 0, srcWidth);
    const int y0 = std::clamp(rect.y, 0, srcHeight);
    const int x1 = std::clamp(rect.Right(), 0, srcWidth);
    const int y1 = std::clamp(rect.Bottom(), 0, srcHeight);
    return CropRect{x0, y0, x1 - x0, y1 - y0};
}

} // namespace

void CropImage(const Image& src, const CropRect& rect, Image& dst) {
    const CropRect clamped = ClampRect(rect, src.Width(), src.Height());
    if (clamped.Empty()) {
        return;
    }
    dst.Resize(clamped.width, clamped.height);
    for (int y = 0; y < clamped.height; ++y) {
        for (int x = 0; x < clamped.width; ++x) {
            dst.SetPixel(x, y, src.GetPixel(clamped.x + x, clamped.y + y));
        }
    }
}

void CropMask(const Mask& src, const CropRect& rect, Mask& dst) {
    const CropRect clamped = ClampRect(rect, src.Width(), src.Height());
    if (clamped.Empty()) {
        return;
    }
    dst.Resize(clamped.width, clamped.height);
    for (int y = 0; y < clamped.height; ++y) {
        for (int x = 0; x < clamped.width; ++x) {
            dst.Set(x, y, src.Get(clamped.x + x, clamped.y + y));
        }
    }
}

} // namespace pixie
