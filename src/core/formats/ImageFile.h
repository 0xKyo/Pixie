#pragma once

#include <filesystem>
#include <string>

#include "core/graphics/Image.h"

namespace pixie::formats {

// Loads PNG, JPEG, BMP, TGA, GIF, PSD, HDR... (stb_image) or the first frame of a .aseprite/.ase.
bool LoadImageFile(const std::filesystem::path& path, graphics::Image& image, std::string* error = nullptr);

bool SavePng(const graphics::Image& image, const std::filesystem::path& path, std::string* error = nullptr);
bool SaveAseprite(const graphics::Image& image, const std::filesystem::path& path, std::string* error = nullptr);

} // namespace pixie::formats
