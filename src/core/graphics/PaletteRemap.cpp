#include "core/graphics/PaletteRemap.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <thread>
#include <unordered_map>
#include <vector>

namespace pixie::graphics {

namespace {

uint32_t PackRgb(color::Rgba pixel) {
    return (static_cast<uint32_t>(pixel.r) << 16) | (static_cast<uint32_t>(pixel.g) << 8) | pixel.b;
}

// Closest palette color to `pixel`; `cache` remembers answers per exact RGB value (one per thread).
color::Rgba FindClosest(color::Rgba pixel, const std::vector<color::PaletteColor>& colors,
                        std::unordered_map<uint32_t, color::Rgba>& cache) {
    const uint32_t key = PackRgb(pixel);
    if (auto cached = cache.find(key); cached != cache.end()) {
        return cached->second;
    }

    const color::Lab pixelLab = color::ToLab(pixel);
    double bestDistance = std::numeric_limits<double>::max();
    color::Rgba bestColor = colors.front().rgb;
    for (const color::PaletteColor& candidate : colors) {
        const double distance = color::Ciede2000(pixelLab, candidate.lab);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestColor = candidate.rgb;
        }
    }
    cache.emplace(key, bestColor);
    return bestColor;
}

} // namespace

void RemapToPalette(const Image& source, const color::Palette& palette, Image& result) {
    result.Resize(source.Width(), source.Height());
    if (source.Empty()) return;

    const std::vector<color::PaletteColor>& colors = palette.Colors();
    const bool hasPalette = !colors.empty();
    const int width = source.Width();
    const int height = source.Height();

    // Each thread takes a horizontal band of rows.
    const unsigned threadCount = std::clamp<unsigned>(std::thread::hardware_concurrency(), 1u, static_cast<unsigned>(height));
    const int rowsPerThread = (height + static_cast<int>(threadCount) - 1) / static_cast<int>(threadCount);

    std::vector<std::thread> workers;
    workers.reserve(threadCount);
    for (unsigned band = 0; band < threadCount; ++band) {
        const int firstRow = static_cast<int>(band) * rowsPerThread;
        const int endRow = std::min(height, firstRow + rowsPerThread);
        if (firstRow >= endRow) continue;
        workers.emplace_back([&source, &result, &colors, hasPalette, width, firstRow, endRow]() {
            std::unordered_map<uint32_t, color::Rgba> cache;
            for (int y = firstRow; y < endRow; ++y) {
                for (int x = 0; x < width; ++x) {
                    const color::Rgba original = source.GetPixel(x, y);
                    color::Rgba remapped = hasPalette ? FindClosest(original, colors, cache) : original;
                    remapped.a = original.a;
                    result.SetPixel(x, y, remapped);
                }
            }
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }
}

void BinarizeAlpha(Image& image) {
    for (int y = 0; y < image.Height(); ++y) {
        for (int x = 0; x < image.Width(); ++x) {
            color::Rgba pixel = image.GetPixel(x, y);
            pixel.a = pixel.a >= 128 ? 255 : 0;
            image.SetPixel(x, y, pixel);
        }
    }
}

void RemoveTransparency(Image& image) {
    for (int y = 0; y < image.Height(); ++y) {
        for (int x = 0; x < image.Width(); ++x) {
            color::Rgba pixel = image.GetPixel(x, y);
            if (pixel.a > 0) {
                pixel.a = 255;
                image.SetPixel(x, y, pixel);
            }
        }
    }
}

} // namespace pixie::graphics
