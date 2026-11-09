#include "core/graphics/Mask.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "core/color/Color.h"

namespace pixie::graphics {

void Mask::Resize(int width, int height) {
    width_ = width;
    height_ = height;
    flags_.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
}

void Mask::Clear() {
    width_ = 0;
    height_ = 0;
    flags_.clear();
}

namespace {

using Point = std::pair<int, int>;

// Breadth-first 4-connected fill from `seeds`; `canSpread(from, to)` decides each step, `visit` marks a pixel.
template <typename CanSpread, typename Visit>
void FloodFill(int width, int height, const std::vector<Point>& seeds, CanSpread canSpread, Visit visit) {
    std::vector<uint8_t> visited(static_cast<size_t>(width) * height, 0);
    std::vector<Point> queue; // indexed, not iterated: push_back may reallocate mid-loop
    queue.reserve(seeds.size());

    for (const auto& [x, y] : seeds) {
        const size_t index = static_cast<size_t>(y) * width + x;
        if (visited[index]) continue;
        visited[index] = 1;
        visit(x, y);
        queue.emplace_back(x, y);
    }

    static constexpr int kStepX[4] = {1, -1, 0, 0};
    static constexpr int kStepY[4] = {0, 0, 1, -1};

    for (size_t head = 0; head < queue.size(); ++head) {
        const auto [fromX, fromY] = queue[head];
        for (int direction = 0; direction < 4; ++direction) {
            const int toX = fromX + kStepX[direction];
            const int toY = fromY + kStepY[direction];
            if (toX < 0 || toX >= width || toY < 0 || toY >= height) continue;
            const size_t index = static_cast<size_t>(toY) * width + toX;
            if (visited[index] || !canSpread(fromX, fromY, toX, toY)) continue;
            visited[index] = 1;
            visit(toX, toY);
            queue.emplace_back(toX, toY);
        }
    }
}

// L*a*b* value of every pixel, computed once so the fill doesn't convert repeatedly.
std::vector<color::Lab> ToLabBuffer(const Image& image) {
    const size_t count = static_cast<size_t>(image.Width()) * static_cast<size_t>(image.Height());
    std::vector<color::Lab> labs(count);
    const color::Rgba* pixels = image.Pixels();
    for (size_t i = 0; i < count; ++i) {
        labs[i] = color::ToLab(pixels[i]);
    }
    return labs;
}

} // namespace

void DetectBackground(const Image& image, double tolerance, Mask& mask) {
    const int width = image.Width();
    const int height = image.Height();
    mask.Resize(width, height);
    if (image.Empty()) return;

    const std::vector<color::Lab> labs = ToLabBuffer(image);

    std::vector<Point> borderPixels;
    borderPixels.reserve(2 * (static_cast<size_t>(width) + height));
    for (int x = 0; x < width; ++x) {
        borderPixels.emplace_back(x, 0);
        borderPixels.emplace_back(x, height - 1);
    }
    for (int y = 0; y < height; ++y) {
        borderPixels.emplace_back(0, y);
        borderPixels.emplace_back(width - 1, y);
    }

    // Spreads by comparing each pixel with its neighbor, so gradual backgrounds are followed too.
    FloodFill(
        width, height, borderPixels,
        [&](int fromX, int fromY, int toX, int toY) {
            const color::Lab& from = labs[static_cast<size_t>(fromY) * width + fromX];
            const color::Lab& to = labs[static_cast<size_t>(toY) * width + toX];
            return color::Ciede2000(from, to) <= tolerance;
        },
        [&](int x, int y) { mask.SetBackground(x, y, true); });
}

void ToggleRegion(const Image& image, int x, int y, double tolerance, Mask& mask) {
    const int width = image.Width();
    const int height = image.Height();
    if (image.Empty() || x < 0 || x >= width || y < 0 || y >= height) return;
    if (!mask.MatchesSize(width, height)) return;

    const std::vector<color::Lab> labs = ToLabBuffer(image);
    const color::Lab clickedColor = labs[static_cast<size_t>(y) * width + x];
    const bool newValue = !mask.IsBackground(x, y);

    // Unlike DetectBackground, every pixel is compared against the clicked color.
    FloodFill(
        width, height, {{x, y}},
        [&](int, int, int toX, int toY) {
            return color::Ciede2000(clickedColor, labs[static_cast<size_t>(toY) * width + toX]) <= tolerance;
        },
        [&](int pixelX, int pixelY) { mask.SetBackground(pixelX, pixelY, newValue); });
}

void ApplyMask(const Mask& mask, Image& image) {
    if (!mask.MatchesSize(image.Width(), image.Height())) return;
    for (int y = 0; y < image.Height(); ++y) {
        for (int x = 0; x < image.Width(); ++x) {
            if (!mask.IsBackground(x, y)) continue;
            color::Rgba pixel = image.GetPixel(x, y);
            pixel.a = 0;
            image.SetPixel(x, y, pixel);
        }
    }
}

} // namespace pixie::graphics
