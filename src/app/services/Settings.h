#pragma once

#include <filesystem>

namespace pixie::services {

// User preferences, saved as "key=value" lines in the per-user app data folder.
struct Settings {
    std::filesystem::path libraryRoot;  // assets folder; empty until the user picks one
    std::filesystem::path audacityPath; // Audacity executable; empty = auto-detect
    float tileSize = 0.0f;              // library thumbnail size; 0 = never saved

    void Load();
    void Save() const;
};

} // namespace pixie::services
