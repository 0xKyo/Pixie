#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "core/ColorMath.h"

namespace pixie {

// A simple RGBA8 pixel buffer, loaded from / saved to disk via stb_image.
class Image {
public:
    Image() = default;

    // Loads any format stb_image supports (PNG, JPEG, BMP, TGA, GIF, PSD, HDR, PIC, PNM, ...).
    bool LoadFromFile(const std::filesystem::path& path, std::string* error = nullptr);

    // Saves as PNG, inferring nothing about `path` — the extension is not modified.
    bool SaveToPng(const std::filesystem::path& path, std::string* error = nullptr) const;

    void Resize(int width, int height);
    void Clear();

    [[nodiscard]] int Width() const { return width_; }
    [[nodiscard]] int Height() const { return height_; }
    [[nodiscard]] bool Empty() const { return pixels_.empty(); }

    [[nodiscard]] const RGB8* Pixels() const { return pixels_.data(); }

    [[nodiscard]] RGB8 GetPixel(int x, int y) const { return pixels_[static_cast<size_t>(y) * width_ + x]; }
    void SetPixel(int x, int y, RGB8 color) { pixels_[static_cast<size_t>(y) * width_ + x] = color; }

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<RGB8> pixels_;
};

} // namespace pixie
