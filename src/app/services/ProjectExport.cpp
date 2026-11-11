#include "app/services/ProjectExport.h"

#include <nfd.hpp>

#include <set>
#include <system_error>

#include "app/util/Platform.h"
#include "core/formats/BinaryFile.h"
#include "core/formats/ImageFile.h"
#include "core/spritesheet/SpriteSheet.h"

namespace pixie::services {

namespace {

constexpr const char* kAudioFolderName = "SFX";

// Asks for the destination folder; empty if cancelled, `dialogError` set if the dialog failed.
std::filesystem::path AskForFolder(const std::filesystem::path& startFolder, std::string& dialogError) {
    NFD::UniquePathN chosen;
    const nfdresult_t result = NFD::PickFolder(chosen, startFolder.empty() ? nullptr : startFolder.c_str());
    if (result == NFD_ERROR) dialogError = std::string("File dialog error: ") + NFD::GetError();
    if (result != NFD_OKAY) return {};
    return std::filesystem::path(chosen.get());
}

// Writes atlas.png + atlas.json; returns a status fragment, or an error starting with "spritesheet failed".
std::string ExportArt(const std::vector<spritesheet::Source>& sources, const std::filesystem::path& folder) {
    const std::filesystem::path atlasPath = folder / "atlas.png";
    const std::filesystem::path layoutPath = folder / "atlas.json";

    spritesheet::Result sheet;
    std::string error;
    if (!spritesheet::Build(sources, spritesheet::Options{}, sheet, &error) ||
        !formats::SavePng(sheet.atlas, atlasPath, &error) ||
        !formats::WriteWholeFile(layoutPath, sheet.layoutJson.data(), sheet.layoutJson.size(), &error)) {
        return "spritesheet failed: " + error;
    }
    return std::to_string(sheet.assetCount) + " sprites / " + std::to_string(sheet.frameCount) + " frames (" +
           std::to_string(sheet.atlas.Width()) + "x" + std::to_string(sheet.atlas.Height()) + ") to " +
           atlasPath.filename().string() + " + " + layoutPath.filename().string();
}

// Copies the audio files into <folder>/SFX; returns how many were copied (errors are appended to `failures`).
size_t ExportAudio(const std::vector<std::filesystem::path>& audioFiles, const std::filesystem::path& folder, std::string& failures) {
    const std::filesystem::path audioFolder = folder / kAudioFolderName;
    std::error_code ec;
    std::filesystem::create_directories(audioFolder, ec);
    if (ec) {
        failures = "could not create " + audioFolder.string() + ": " + ec.message();
        return 0;
    }

    size_t copied = 0;
    std::set<std::filesystem::path> usedNames;
    for (const std::filesystem::path& source : audioFiles) {
        // Same file name from different folders gets _2, _3...
        std::filesystem::path name = source.filename();
        for (int suffix = 2; !usedNames.insert(name).second; ++suffix) {
            name = source.stem() += "_" + std::to_string(suffix) + source.extension().string();
        }
        std::filesystem::copy_file(source, audioFolder / name, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            failures += (failures.empty() ? "" : "; ") + source.filename().string() + ": " + ec.message();
        } else {
            ++copied;
        }
    }
    return copied;
}

} // namespace

std::string ExportProject(const Project& project, const std::vector<library::Asset>& assets, const std::filesystem::path& libraryRoot) {
    // Names must be unique in the layout, so same-named assets from other folders get _2, _3...
    std::vector<spritesheet::Source> sources;
    std::vector<std::filesystem::path> audioFiles;
    std::set<std::string> usedNames;
    size_t notConvertedCount = 0;
    for (const library::Asset& asset : assets) {
        if (!project.assetKeys.count(asset.key)) continue;
        if (asset.IsAudio()) {
            audioFiles.push_back(asset.audioPath);
            continue;
        }
        if (!asset.HasAseprite()) {
            ++notConvertedCount;
            continue;
        }
        std::string uniqueName = asset.name;
        for (int suffix = 2; !usedNames.insert(uniqueName).second; ++suffix) uniqueName = asset.name + "_" + std::to_string(suffix);
        sources.push_back(spritesheet::Source{uniqueName, asset.asepritePath});
    }
    if (sources.empty() && audioFiles.empty()) return "Nothing to export: the project has no converted (.aseprite) or audio assets";

    std::string dialogError;
    const std::filesystem::path folder = AskForFolder(libraryRoot, dialogError);
    if (folder.empty()) return dialogError;

    std::vector<std::string> parts;
    if (!sources.empty()) parts.push_back(ExportArt(sources, folder));
    if (!audioFiles.empty()) {
        std::string failures;
        const size_t copied = ExportAudio(audioFiles, folder, failures);
        parts.push_back(std::to_string(copied) + " audio file(s) to " + kAudioFolderName + "/" + (failures.empty() ? "" : " (failed: " + failures + ")"));
    }

    std::string status = "Exported ";
    for (size_t i = 0; i < parts.size(); ++i) status += (i ? " + " : "") + parts[i];
    if (notConvertedCount > 0) status += " - " + std::to_string(notConvertedCount) + " asset(s) skipped (not converted yet)";
    return status;
}

} // namespace pixie::services
