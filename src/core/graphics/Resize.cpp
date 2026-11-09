#include "core/graphics/Resize.h"

#include <algorithm>
#include <cstdint>

namespace pixie::graphics {

namespace {

// The source index whose pixel center is closest to output index `outputIndex`.
int NearestSourceIndex(int outputIndex, int sourceSize, int outputSize) {
    const double sourcePosition = (outputIndex + 0.5) * sourceSize / static_cast<double>(outputSize);
    return std::clamp(static_cast<int>(sourcePosition), 0, sourceSize - 1);
}

} // namespace

void ResizeNearest(const Image& source, int width, int height, Image& result) {
    if (source.Empty() || width <= 0 || height <= 0) return;
    result.Resize(width, height);
    for (int y = 0; y < height; ++y) {
        const int sourceY = NearestSourceIndex(y, source.Height(), height);
        for (int x = 0; x < width; ++x) {
            const int sourceX = NearestSourceIndex(x, source.Width(), width);
            result.SetPixel(x, y, source.GetPixel(sourceX, sourceY));
        }
    }
}

Image MakeThumbnail(const Image& source, int maxSide) {
    const int sourceWidth = source.Width();
    const int sourceHeight = source.Height();
    if (source.Empty() || maxSide <= 0 || (sourceWidth <= maxSide && sourceHeight <= maxSide)) {
        return source;
    }

    const double scale = static_cast<double>(maxSide) / std::max(sourceWidth, sourceHeight);
    const int width = std::max(1, static_cast<int>(sourceWidth * scale));
    const int height = std::max(1, static_cast<int>(sourceHeight * scale));

    Image thumbnail;
    thumbnail.Resize(width, height);
    for (int y = 0; y < height; ++y) {
        const int firstRow = static_cast<int>(static_cast<int64_t>(y) * sourceHeight / height);
        const int lastRow = std::max(firstRow + 1, static_cast<int>(static_cast<int64_t>(y + 1) * sourceHeight / height));
        for (int x = 0; x < width; ++x) {
            const int firstColumn = static_cast<int>(static_cast<int64_t>(x) * sourceWidth / width);
            const int lastColumn = std::max(firstColumn + 1, static_cast<int>(static_cast<int64_t>(x + 1) * sourceWidth / width));

            // Average the block, weighting colors by alpha so transparent pixels don't darken edges.
            uint64_t red = 0, green = 0, blue = 0, alpha = 0, pixelCount = 0;
            for (int sourceY = firstRow; sourceY < lastRow && sourceY < sourceHeight; ++sourceY) {
                for (int sourceX = firstColumn; sourceX < lastColumn && sourceX < sourceWidth; ++sourceX) {
                    const color::Rgba pixel = source.GetPixel(sourceX, sourceY);
                    red += static_cast<uint64_t>(pixel.r) * pixel.a;
                    green += static_cast<uint64_t>(pixel.g) * pixel.a;
                    blue += static_cast<uint64_t>(pixel.b) * pixel.a;
                    alpha += pixel.a;
                    ++pixelCount;
                }
            }
            if (alpha == 0 || pixelCount == 0) {
                thumbnail.SetPixel(x, y, color::kTransparent);
            } else {
                thumbnail.SetPixel(x, y, color::Rgba{static_cast<uint8_t>(red / alpha), static_cast<uint8_t>(green / alpha),
                                                     static_cast<uint8_t>(blue / alpha), static_cast<uint8_t>(alpha / pixelCount)});
            }
        }
    }
    return thumbnail;
}

} // namespace pixie::graphics
