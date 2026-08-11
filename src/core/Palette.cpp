#include "core/Palette.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace pixie {

namespace {

std::string Trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

} // namespace

void Palette::Clear() {
    name_.clear();
    sourcePath_.clear();
    colors_.clear();
}

bool Palette::LoadFromGpl(const std::filesystem::path& path, std::string* error) {
    std::ifstream file(path, std::ios::in);
    if (!file) {
        if (error) *error = "Could not open file: " + path.string();
        return false;
    }

    std::string line;
    if (!std::getline(file, line)) {
        if (error) *error = "Empty file: " + path.string();
        return false;
    }
    line = Trim(line);
    // Strip an optional UTF-8 BOM some editors add to the first line.
    if (line.size() >= 3 &&
        static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB &&
        static_cast<unsigned char>(line[2]) == 0xBF) {
        line.erase(0, 3);
    }
    if (line.find("GIMP Palette") == std::string::npos) {
        if (error) *error = "Not a GIMP palette file (missing 'GIMP Palette' header): " + path.string();
        return false;
    }

    std::vector<PaletteColor> parsedColors;
    std::string parsedName = path.stem().string();

    while (std::getline(file, line)) {
        std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }
        if (trimmed.rfind("Name:", 0) == 0) {
            parsedName = Trim(trimmed.substr(5));
            continue;
        }
        if (trimmed.rfind("Columns:", 0) == 0) {
            continue; // Layout hint only; not needed for color conversion.
        }

        std::istringstream iss(trimmed);
        int r = 0, g = 0, b = 0;
        if (!(iss >> r >> g >> b)) {
            continue; // Skip malformed/unrecognized lines rather than aborting the whole file.
        }
        r = std::clamp(r, 0, 255);
        g = std::clamp(g, 0, 255);
        b = std::clamp(b, 0, 255);

        std::string rest;
        std::getline(iss, rest);
        rest = Trim(rest);

        PaletteColor pc;
        pc.rgb = RGB8{static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), 255};
        pc.lab = RgbToLab(pc.rgb);
        pc.name = std::move(rest);
        parsedColors.push_back(std::move(pc));
    }

    if (parsedColors.empty()) {
        if (error) *error = "Palette contains no colors: " + path.string();
        return false;
    }

    name_ = std::move(parsedName);
    sourcePath_ = path;
    colors_ = std::move(parsedColors);
    return true;
}

} // namespace pixie
