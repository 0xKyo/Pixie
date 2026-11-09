#include "app/AppContext.h"

namespace pixie::app {

void AppContext::RescanLibrary() {
    if (settings.libraryRoot.empty()) {
        assets.clear();
        return;
    }
    assets = library::ScanFolder(settings.libraryRoot);

    std::set<std::string> existingKeys;
    for (const library::Asset& asset : assets) existingKeys.insert(asset.key);
    for (auto key = selectedKeys.begin(); key != selectedKeys.end();) {
        key = existingKeys.count(*key) ? std::next(key) : selectedKeys.erase(key);
    }
}

void AppContext::OpenLibrary(const std::filesystem::path& root) {
    settings.libraryRoot = root;
    settings.Save();
    thumbnails.Clear();
    selectedKeys.clear();
    RescanLibrary();
    projects.Load(root);
    screen = Screen::Library;
    statusMessage = "Library: " + root.string() + " (" + std::to_string(assets.size()) + " assets)";
}

bool AppContext::OpenSavedLibrary() {
    std::error_code error;
    if (settings.libraryRoot.empty() || !std::filesystem::is_directory(settings.libraryRoot, error)) return false;
    RescanLibrary();
    projects.Load(settings.libraryRoot);
    screen = Screen::Library;
    return true;
}

} // namespace pixie::app
