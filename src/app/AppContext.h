#pragma once

#include <SDL3/SDL.h>

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "app/services/ProjectStore.h"
#include "app/services/Settings.h"
#include "app/services/ThumbnailCache.h"
#include "core/library/Asset.h"

namespace pixie::app {

// Which full-window screen is showing.
enum class Screen { FolderPicker, Library, Converter };

// State shared by every view; the Application owns it and the views keep a reference.
struct AppContext {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    services::Settings settings;
    services::ProjectStore projects;
    services::ThumbnailCache thumbnails;

    std::vector<library::Asset> assets;  // the scanned library, sorted by folder and name
    std::set<std::string> selectedKeys;  // asset keys picked with Ctrl+click
    std::string statusMessage;           // one line of feedback shown at the top
    Screen screen = Screen::FolderPicker;

    // Re-reads the library folder (turning any MP3 into a WAV first); returns how many MP3s were converted.
    size_t RescanLibrary();
    // Switches to a new library folder: saves it, rescans, loads its projects and shows the grid.
    void OpenLibrary(const std::filesystem::path& root);
    // Opens the library folder remembered in settings; false if there is none or it's gone.
    bool OpenSavedLibrary();
};

} // namespace pixie::app
