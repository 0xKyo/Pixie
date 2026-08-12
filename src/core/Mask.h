#pragma once

#include <cstdint>
#include <vector>

#include "core/Image.h"

namespace pixie {

// Per-pixel background/subject flag (false = subject, true = background).
// uint8_t-backed to avoid std::vector<bool>'s bit-packing/proxy pitfalls.
class Mask {
public:
    void Resize(int width, int height); // fills with false (subject)
    void Clear();

    [[nodiscard]] int Width() const { return width_; }
    [[nodiscard]] int Height() const { return height_; }
    [[nodiscard]] bool Empty() const { return values_.empty(); }
    [[nodiscard]] bool MatchesSize(int width, int height) const {
        return width_ == width && height_ == height;
    }

    [[nodiscard]] bool Get(int x, int y) const { return values_[static_cast<size_t>(y) * width_ + x] != 0; }
    void Set(int x, int y, bool background) { values_[static_cast<size_t>(y) * width_ + x] = background ? 1 : 0; }

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<uint8_t> values_;
};

// Flood-fills background inward from the border: a neighbor joins iff its
// CIEDE2000 distance to the already-background pixel is <= tolerance.
void ComputeBackgroundMask(const Image& src, double tolerance, Mask& outMask);

// Magic-wand toggle: flood-fills from (x, y) by CIEDE2000 distance to the
// clicked color, flipping every reached pixel. No-op if mask doesn't match src's size.
void ToggleRegion(const Image& src, int x, int y, double tolerance, Mask& mask);

// Sets alpha=0 on every background-masked pixel; leaves RGB and non-masked
// pixels untouched. No-op if `mask` and `image` sizes don't match.
void ApplyBackgroundMask(const Mask& mask, Image& image);

} // namespace pixie
