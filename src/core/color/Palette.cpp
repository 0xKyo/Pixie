#include "core/color/Palette.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace pixie::color {

namespace {

std::string Trim(std::string text) {
    auto isNotSpace = [](unsigned char c) { return !std::isspace(c); };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), isNotSpace));
    text.erase(std::find_if(text.rbegin(), text.rend(), isNotSpace).base(), text.end());
    return text;
}

// Some editors put a UTF-8 byte order mark at the start of the file.
void StripUtf8Bom(std::string& line) {
    if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF) {
        line.erase(0, 3);
    }
}

} // namespace

void Palette::Clear() {
    name_.clear();
    colors_.clear();
}

bool Palette::LoadFromGpl(const std::filesystem::path& path, std::string* error) {
    auto fail = [&](const std::string& message) {
        if (error) *error = message + ": " + path.string();
        return false;
    };

    std::ifstream file(path, std::ios::in);
    if (!file) return fail("Could not open file");

    std::string line;
    if (!std::getline(file, line)) return fail("Empty file");
    line = Trim(line);
    StripUtf8Bom(line);
    if (line.find("GIMP Palette") == std::string::npos) {
        return fail("Not a GIMP palette file (missing 'GIMP Palette' header)");
    }

    std::vector<PaletteColor> parsedColors;
    std::string parsedName = path.stem().string();

    while (std::getline(file, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed.front() == '#') continue;
        if (trimmed.rfind("Name:", 0) == 0) {
            parsedName = Trim(trimmed.substr(5));
            continue;
        }
        if (trimmed.rfind("Columns:", 0) == 0) continue; // layout hint only

        // Color lines look like "R G B optional name"; anything else is skipped.
        std::istringstream fields(trimmed);
        int red = 0, green = 0, blue = 0;
        if (!(fields >> red >> green >> blue)) continue;

        std::string colorName;
        std::getline(fields, colorName);

        PaletteColor entry;
        entry.rgb = Rgba{static_cast<uint8_t>(std::clamp(red, 0, 255)), static_cast<uint8_t>(std::clamp(green, 0, 255)),
                         static_cast<uint8_t>(std::clamp(blue, 0, 255)), 255};
        entry.lab = ToLab(entry.rgb);
        entry.name = Trim(colorName);
        parsedColors.push_back(std::move(entry));
    }

    if (parsedColors.empty()) return fail("Palette contains no colors");

    name_ = std::move(parsedName);
    colors_ = std::move(parsedColors);
    return true;
}

} // namespace pixie::color
