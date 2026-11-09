#pragma once

#include <cstdint>
#include <vector>

#include "core/graphics/Image.h"

namespace pixie::graphics {

// One flag per pixel: true = background (to be made transparent), false = subject.
class Mask {
public:
    void Resize(int width, int height); // every pixel starts as subject
    void Clear();

    [[nodiscard]] int Width() const { return width_; }
    [[nodiscard]] int Height() const { return height_; }
    [[nodiscard]] bool Empty() const { return flags_.empty(); }
    [[nodiscard]] bool MatchesSize(int width, int height) const { return width_ == width && height_ == height; }

    [[nodiscard]] bool IsBackground(int x, int y) const { return flags_[Index(x, y)] != 0; }
    void SetBackground(int x, int y, bool background) { flags_[Index(x, y)] = background ? 1 : 0; }

private:
    [[nodiscard]] size_t Index(int x, int y) const { return static_cast<size_t>(y) * width_ + x; }

    int width_ = 0;
    int height_ = 0;
    std::vector<uint8_t> flags_; // uint8_t instead of vector<bool> to avoid its proxy quirks
};

// Flood-fills from the image border, marking neighbors within `tolerance` (CIEDE2000) as background.
void DetectBackground(const Image& image, double tolerance, Mask& mask);

// Magic wand: flips every pixel connected to (x, y) whose color is within `tolerance` of the clicked one.
void ToggleRegion(const Image& image, int x, int y, double tolerance, Mask& mask);

// Sets alpha to 0 on every background pixel; does nothing if the sizes differ.
void ApplyMask(const Mask& mask, Image& image);

} // namespace pixie::graphics
