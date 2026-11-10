#include "app/AppContext.h"

#include "core/audio/AudioClip.h"
#include "core/audio/WavWrite.h"

namespace pixie::app {

namespace {

// Decodes every MP3 to a .wav beside it and deletes the MP3; returns old key -> new .wav path.
std::vector<std::pair<std::string, std::filesystem::path>> ConvertMp3sToWav(const std::vector<library::Asset>& assets, std::string& failures) {
    std::vector<std::pair<std::string, std::filesystem::path>> converted;
    for (const library::Asset& asset : assets) {
        if (!asset.IsAudio() || asset.audioFormat != "MP3") continue;
        std::filesystem::path wavPath = asset.audioPath;
        wavPath.replace_extension(".wav");
        std::string error;
        audio::AudioClip clip;
        if (!std::filesystem::exists(wavPath) && (!audio::LoadClip(asset.audioPath, clip, &error) || !audio::SaveWav(clip, wavPath, &error))) {
            failures += (failures.empty() ? "" : "; ") + asset.audioPath.filename().string() + ": " + error;
            continue; // keep the MP3 if the WAV could not be written
        }
        std::error_code removeError;
        std::filesystem::remove(asset.audioPath, removeError);
        converted.emplace_back(asset.key, wavPath);
    }
    return converted;
}

} // namespace

size_t AppContext::RescanLibrary() {
    if (settings.libraryRoot.empty()) {
        assets.clear();
        return 0;
    }
    assets = library::ScanFolder(settings.libraryRoot);

    std::string failures;
    const auto converted = ConvertMp3sToWav(assets, failures);
    if (!converted.empty()) {
        assets = library::ScanFolder(settings.libraryRoot);
        for (const auto& [oldKey, wavPath] : converted) {
            for (const library::Asset& asset : assets) {
                if (asset.IsAudio() && asset.audioPath == wavPath) projects.RenameAssetKey(oldKey, asset.key); // projects follow the file
            }
        }
        statusMessage = "Converted " + std::to_string(converted.size()) + " MP3 to WAV (MP3 deleted)";
    }
    if (!failures.empty()) statusMessage = "MP3 conversion failed: " + failures;

    std::set<std::string> existingKeys;
    for (const library::Asset& asset : assets) existingKeys.insert(asset.key);
    for (auto key = selectedKeys.begin(); key != selectedKeys.end();) {
        key = existingKeys.count(*key) ? std::next(key) : selectedKeys.erase(key);
    }
    return converted.size();
}

void AppContext::OpenLibrary(const std::filesystem::path& root) {
    settings.libraryRoot = root;
    settings.Save();
    thumbnails.Clear();
    selectedKeys.clear();
    projects.Load(root);
    RescanLibrary();
    screen = Screen::Library;
    statusMessage = "Library: " + root.string() + " (" + std::to_string(assets.size()) + " assets)";
}

bool AppContext::OpenSavedLibrary() {
    std::error_code error;
    if (settings.libraryRoot.empty() || !std::filesystem::is_directory(settings.libraryRoot, error)) return false;
    projects.Load(settings.libraryRoot);
    RescanLibrary();
    screen = Screen::Library;
    return true;
}

} // namespace pixie::app
