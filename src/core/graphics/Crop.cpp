#include "core/graphics/Crop.h"

#include <algorithm>
#include <cmath>

namespace pixie::graphics {

namespace {

CropRect ClampToImage(const CropRect& rect, int imageWidth, int imageHeight) {
    const int left = std::clamp(rect.x, 0, imageWidth);
    const int top = std::clamp(rect.y, 0, imageHeight);
    const int right = std::clamp(rect.Right(), 0, imageWidth);
    const int bottom = std::clamp(rect.Bottom(), 0, imageHeight);
    return CropRect{left, top, right - left, bottom - top};
}

} // namespace

void CropImage(const Image& source, const CropRect& rect, Image& result) {
    const CropRect area = ClampToImage(rect, source.Width(), source.Height());
    if (area.Empty()) return;
    result.Resize(area.width, area.height);
    for (int y = 0; y < area.height; ++y) {
        for (int x = 0; x < area.width; ++x) {
            result.SetPixel(x, y, source.GetPixel(area.x + x, area.y + y));
        }
    }
}

void CropMask(const Mask& source, const CropRect& rect, Mask& result) {
    const CropRect area = ClampToImage(rect, source.Width(), source.Height());
    if (area.Empty()) return;
    result.Resize(area.width, area.height);
    for (int y = 0; y < area.height; ++y) {
        for (int x = 0; x < area.width; ++x) {
            result.SetBackground(x, y, source.IsBackground(area.x + x, area.y + y));
        }
    }
}

CropRect CenteredRectWithAspect(int imageWidth, int imageHeight, int targetWidth, int targetHeight) {
    if (imageWidth <= 0 || imageHeight <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        return CropRect{};
    }
    const double imageAspect = static_cast<double>(imageWidth) / imageHeight;
    const double targetAspect = static_cast<double>(targetWidth) / targetHeight;

    // A wider image keeps its full height; a taller one keeps its full width.
    int width = imageWidth;
    int height = imageHeight;
    if (imageAspect > targetAspect) {
        width = std::max(1, static_cast<int>(std::lround(imageHeight * targetAspect)));
    } else {
        height = std::max(1, static_cast<int>(std::lround(imageWidth / targetAspect)));
    }
    width = std::min(width, imageWidth);
    height = std::min(height, imageHeight);
    return CropRect{(imageWidth - width) / 2, (imageHeight - height) / 2, width, height};
}

} // namespace pixie::graphics
