#include "app/services/Settings.h"

#include <SDL3/SDL.h>

#include <fstream>
#include <string>

#include "app/util/Text.h"

namespace pixie::services {

namespace {

constexpr const char* kLibraryRootKey = "library_root=";
constexpr const char* kTileSizeKey = "tile_size=";
constexpr const char* kAudacityPathKey = "audacity_path=";

std::string ToUtf8(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

// e.g. %APPDATA%/Pixie/Pixie/settings.txt on Windows.
std::filesystem::path SettingsFile() {
    char* folder = SDL_GetPrefPath("Pixie", "Pixie");
    if (!folder) return {};
    std::filesystem::path file = util::Utf8Path(folder) / "settings.txt";
    SDL_free(folder);
    return file;
}

bool StartsWith(const std::string& line, const char* prefix) { return line.rfind(prefix, 0) == 0; }

std::string ValueAfter(const std::string& line, const char* prefix) { return line.substr(std::string(prefix).size()); }

} // namespace

void Settings::Load() {
    const std::filesystem::path file = SettingsFile();
    if (file.empty()) return;
    std::ifstream input(file, std::ios::binary);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (StartsWith(line, kLibraryRootKey)) {
            libraryRoot = util::Utf8Path(ValueAfter(line, kLibraryRootKey));
        } else if (StartsWith(line, kAudacityPathKey)) {
            audacityPath = util::Utf8Path(ValueAfter(line, kAudacityPathKey));
        } else if (StartsWith(line, kTileSizeKey)) {
            try {
                tileSize = std::stof(ValueAfter(line, kTileSizeKey));
            } catch (...) {
                tileSize = 0.0f;
            }
        }
    }
}

void Settings::Save() const {
    const std::filesystem::path file = SettingsFile();
    if (file.empty()) return;
    std::ofstream output(file, std::ios::binary | std::ios::trunc);
    output << kLibraryRootKey << ToUtf8(libraryRoot) << "\n";
    if (tileSize > 0.0f) output << kTileSizeKey << tileSize << "\n";
    if (!audacityPath.empty()) output << kAudacityPathKey << ToUtf8(audacityPath) << "\n";
}

} // namespace pixie::services
