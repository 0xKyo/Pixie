#pragma once

#include <cstddef>
#include <vector>

#include "core/color/Color.h"

namespace pixie::graphics {

// A width x height grid of RGBA pixels stored row by row (file IO lives in core/formats).
class Image {
public:
    Image() = default;

    // Reallocates to the new size with every pixel set to `fill` (opaque black by default).
    void Resize(int width, int height, color::Rgba fill = {});
    void Clear();

    [[nodiscard]] int Width() const { return width_; }
    [[nodiscard]] int Height() const { return height_; }
    [[nodiscard]] bool Empty() const { return pixels_.empty(); }

    // Tightly packed RGBA8 bytes, suitable for uploading or encoding.
    [[nodiscard]] const color::Rgba* Pixels() const { return pixels_.data(); }
    [[nodiscard]] color::Rgba* MutablePixels() { return pixels_.data(); }

    [[nodiscard]] color::Rgba GetPixel(int x, int y) const { return pixels_[Index(x, y)]; }
    void SetPixel(int x, int y, color::Rgba pixel) { pixels_[Index(x, y)] = pixel; }

private:
    [[nodiscard]] size_t Index(int x, int y) const { return static_cast<size_t>(y) * width_ + x; }

    int width_ = 0;
    int height_ = 0;
    std::vector<color::Rgba> pixels_;
};

} // namespace pixie::graphics
