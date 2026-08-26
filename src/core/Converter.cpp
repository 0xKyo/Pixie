#include "core/Converter.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <thread>
#include <unordered_map>
#include <vector>

namespace pixie {

namespace {

uint32_t PackRgb(RGB8 c) {
    return (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | static_cast<uint32_t>(c.b);
}

// Finds the palette color closest to `pixel` under CIEDE2000, memoizing by
// exact RGB value in `cache` (per-thread, unsynchronized).
RGB8 FindNearest(RGB8 pixel, const std::vector<PaletteColor>& palette, std::unordered_map<uint32_t, RGB8>& cache) {
    const uint32_t key = PackRgb(pixel);
    if (auto it = cache.find(key); it != cache.end()) {
        return it->second;
    }

    const Lab lab = RgbToLab(pixel);
    double bestDist = std::numeric_limits<double>::max();
    RGB8 best = palette.front().rgb;
    for (const PaletteColor& candidate : palette) {
        const double dist = Ciede2000(lab, candidate.lab);
        if (dist < bestDist) {
            bestDist = dist;
            best = candidate.rgb;
        }
    }
    cache.emplace(key, best);
    return best;
}

} // namespace

void ConvertImage(const Image& src, const Palette& palette, Image& dst, const ConvertOptions& opts) {
    dst.Resize(src.Width(), src.Height());
    if (src.Empty()) {
        return;
    }

    const std::vector<PaletteColor>& colors = palette.Colors();
    const bool hasPalette = !colors.empty();
    const int width = src.Width();
    const int height = src.Height();

    unsigned threadCount = opts.threadCount != 0 ? opts.threadCount : std::thread::hardware_concurrency();
    threadCount = std::clamp<unsigned>(threadCount, 1u, static_cast<unsigned>(std::max(height, 1)));

    const int rowsPerThread = (height + static_cast<int>(threadCount) - 1) / static_cast<int>(threadCount);

    std::vector<std::thread> workers;
    workers.reserve(threadCount);

    for (unsigned t = 0; t < threadCount; ++t) {
        const int rowStart = static_cast<int>(t) * rowsPerThread;
        const int rowEnd = std::min(height, rowStart + rowsPerThread);
        if (rowStart >= rowEnd) {
            continue;
        }
        workers.emplace_back([&src, &dst, &colors, hasPalette, width, rowStart, rowEnd]() {
            std::unordered_map<uint32_t, RGB8> cache;
            for (int y = rowStart; y < rowEnd; ++y) {
                for (int x = 0; x < width; ++x) {
                    const RGB8 srcPixel = src.GetPixel(x, y);
                    RGB8 out = hasPalette ? FindNearest(srcPixel, colors, cache) : srcPixel;
                    out.a = srcPixel.a;
                    dst.SetPixel(x, y, out);
                }
            }
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }
}

void BinarizeAlpha(Image& image) {
    const int width = image.Width();
    const int height = image.Height();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            RGB8 pixel = image.GetPixel(x, y);
            pixel.a = pixel.a >= 128 ? 255 : 0;
            image.SetPixel(x, y, pixel);
        }
    }
}

} // namespace pixie
