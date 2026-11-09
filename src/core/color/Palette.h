#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/color/Color.h"

namespace pixie::color {

// One palette entry, with its L*a*b* value precomputed for fast matching.
struct PaletteColor {
    Rgba rgb;
    Lab lab;
    std::string name;
};

// An ordered list of colors loaded from a GIMP palette (.gpl) file.
class Palette {
public:
    // Replaces this palette with the file's colors; on failure keeps it and fills `error`.
    bool LoadFromGpl(const std::filesystem::path& path, std::string* error = nullptr);
    void Clear();

    [[nodiscard]] bool Empty() const { return colors_.empty(); }
    [[nodiscard]] const std::vector<PaletteColor>& Colors() const { return colors_; }
    [[nodiscard]] const std::string& Name() const { return name_; }

private:
    std::string name_;
    std::vector<PaletteColor> colors_;
};

} // namespace pixie::color
