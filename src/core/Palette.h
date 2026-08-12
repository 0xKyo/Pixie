#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/ColorMath.h"

namespace pixie {

struct PaletteColor {
    RGB8 rgb;
    Lab lab;
    std::string name;
};

// A list of colors loaded from a palette file (currently GIMP .gpl only).
class Palette {
public:
    // Parses a GIMP palette (.gpl) file. On failure returns false and, if
    // `error` is non-null, fills it with a human-readable message.
    bool LoadFromGpl(const std::filesystem::path& path, std::string* error = nullptr);

    void Clear();

    [[nodiscard]] bool Empty() const { return colors_.empty(); }
    [[nodiscard]] const std::vector<PaletteColor>& Colors() const { return colors_; }
    [[nodiscard]] const std::string& Name() const { return name_; }

private:
    std::string name_;
    std::vector<PaletteColor> colors_;
};

} // namespace pixie
