#include "core/Mask.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "core/ColorMath.h"

namespace pixie {

void Mask::Resize(int width, int height) {
    width_ = width;
    height_ = height;
    values_.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
}

void Mask::Clear() {
    width_ = 0;
    height_ = 0;
    values_.clear();
}

namespace {

// Iterative 4-connected BFS from `seeds`; a neighbor joins iff
// `shouldJoin(cx, cy, nx, ny)` is true. `visited` is caller-provided scratch space.
//
// Indexed by size_t rather than iterator: emplace_back may reallocate
// `queue` mid-loop, which would invalidate a cached iterator.
template <typename ShouldJoin, typename OnVisit>
void FloodFillBfs(int width, int height, const std::vector<std::pair<int, int>>& seeds,
                   std::vector<uint8_t>& visited, ShouldJoin shouldJoin, OnVisit onVisit) {
    std::fill(visited.begin(), visited.end(), 0);
    std::vector<std::pair<int, int>> queue;
    queue.reserve(seeds.size());

    auto tryVisit = [&](int x, int y) {
        const size_t idx = static_cast<size_t>(y) * width + x;
        if (visited[idx]) return;
        visited[idx] = 1;
        onVisit(x, y);
        queue.emplace_back(x, y);
    };

    for (const auto& [sx, sy] : seeds) {
        tryVisit(sx, sy);
    }

    static constexpr int kDx[4] = {1, -1, 0, 0};
    static constexpr int kDy[4] = {0, 0, 1, -1};

    for (size_t head = 0; head < queue.size(); ++head) {
        const auto [cx, cy] = queue[head];
        for (int d = 0; d < 4; ++d) {
            const int nx = cx + kDx[d];
            const int ny = cy + kDy[d];
            if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
            const size_t nidx = static_cast<size_t>(ny) * width + nx;
            if (visited[nidx]) continue;
            if (!shouldJoin(cx, cy, nx, ny)) continue;
            visited[nidx] = 1;
            onVisit(nx, ny);
            queue.emplace_back(nx, ny);
        }
    }
}

std::vector<Lab> ComputeLabBuffer(const Image& src) {
    const size_t count = static_cast<size_t>(src.Width()) * static_cast<size_t>(src.Height());
    std::vector<Lab> labs(count);
    const RGB8* pixels = src.Pixels();
    for (size_t i = 0; i < count; ++i) {
        labs[i] = RgbToLab(pixels[i]);
    }
    return labs;
}

} // namespace

void ComputeBackgroundMask(const Image& src, double tolerance, Mask& outMask) {
    const int width = src.Width();
    const int height = src.Height();
    outMask.Resize(width, height);
    if (src.Empty()) {
        return;
    }

    const std::vector<Lab> labs = ComputeLabBuffer(src);

    std::vector<std::pair<int, int>> seeds;
    seeds.reserve(static_cast<size_t>(2) * (static_cast<size_t>(width) + static_cast<size_t>(height)));
    for (int x = 0; x < width; ++x) {
        seeds.emplace_back(x, 0);
        seeds.emplace_back(x, height - 1);
    }
    for (int y = 0; y < height; ++y) {
        seeds.emplace_back(0, y);
        seeds.emplace_back(width - 1, y);
    }

    std::vector<uint8_t> visited(labs.size());
    FloodFillBfs(
        width, height, seeds, visited,
        [&](int cx, int cy, int nx, int ny) {
            const Lab& a = labs[static_cast<size_t>(cy) * width + cx];
            const Lab& b = labs[static_cast<size_t>(ny) * width + nx];
            return Ciede2000(a, b) <= tolerance;
        },
        [&](int x, int y) { outMask.Set(x, y, true); });
}

void ToggleRegion(const Image& src, int x, int y, double tolerance, Mask& mask) {
    const int width = src.Width();
    const int height = src.Height();
    if (src.Empty() || x < 0 || x >= width || y < 0 || y >= height) {
        return;
    }
    if (!mask.MatchesSize(width, height)) {
        return;
    }

    const std::vector<Lab> labs = ComputeLabBuffer(src);
    const Lab clickedLab = labs[static_cast<size_t>(y) * width + x];
    const bool newValue = !mask.Get(x, y);

    std::vector<uint8_t> visited(labs.size());
    FloodFillBfs(
        width, height, {{x, y}}, visited,
        [&](int, int, int nx, int ny) {
            const Lab& b = labs[static_cast<size_t>(ny) * width + nx];
            return Ciede2000(clickedLab, b) <= tolerance;
        },
        [&](int px, int py) { mask.Set(px, py, newValue); });
}

void ApplyBackgroundMask(const Mask& mask, Image& image) {
    if (mask.Width() != image.Width() || mask.Height() != image.Height()) {
        return;
    }
    for (int y = 0; y < image.Height(); ++y) {
        for (int x = 0; x < image.Width(); ++x) {
            if (mask.Get(x, y)) {
                RGB8 p = image.GetPixel(x, y);
                p.a = 0;
                image.SetPixel(x, y, p);
            }
        }
    }
}

} // namespace pixie
