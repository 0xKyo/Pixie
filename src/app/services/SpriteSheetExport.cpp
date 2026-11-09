#include "app/services/SpriteSheetExport.h"

#include <nfd.hpp>

#include <set>

#include "app/util/Platform.h"
#include "app/util/Text.h"
#include "core/formats/BinaryFile.h"
#include "core/formats/ImageFile.h"
#include "core/spritesheet/SpriteSheet.h"

namespace pixie::services {

namespace {

// Asks for the atlas PNG path; empty if cancelled, `dialogError` set if the dialog failed.
std::filesystem::path AskForAtlasPath(const std::string& projectName, const std::filesystem::path& startFolder,
                                      std::string& dialogError) {
    NFD::UniquePathN chosen;
    const nfdnfilteritem_t filters[] = {{PIXIE_NSTR("PNG Image"), PIXIE_NSTR("png")}};
    const util::NativeString suggestedName = (util::Utf8Path(projectName) += "_atlas.png").native();
    const nfdresult_t result = NFD::SaveDialog(chosen, filters, 1, startFolder.c_str(), suggestedName.c_str());
    if (result == NFD_ERROR) dialogError = std::string("File dialog error: ") + NFD::GetError();
    if (result != NFD_OKAY) return {};

    std::filesystem::path atlasPath(chosen.get());
    if (atlasPath.extension().empty()) atlasPath += ".png";
    return atlasPath;
}

} // namespace

std::string ExportSpriteSheet(const Project& project, const std::vector<library::Asset>& assets,
                              const std::filesystem::path& libraryRoot) {
    // Names must be unique in the layout, so same-named assets from other folders get _2, _3...
    std::vector<spritesheet::Source> sources;
    std::set<std::string> usedNames;
    size_t notConvertedCount = 0;
    for (const library::Asset& asset : assets) {
        if (!project.assetKeys.count(asset.key) || asset.IsAudio()) continue;
        if (!asset.HasAseprite()) {
            ++notConvertedCount;
            continue;
        }
        std::string uniqueName = asset.name;
        for (int suffix = 2; !usedNames.insert(uniqueName).second; ++suffix) uniqueName = asset.name + "_" + std::to_string(suffix);
        sources.push_back(spritesheet::Source{uniqueName, asset.asepritePath});
    }
    if (sources.empty()) return "Nothing to export: the project has no converted (.aseprite) assets";

    std::string dialogError;
    const std::filesystem::path atlasPath = AskForAtlasPath(project.name, libraryRoot, dialogError);
    if (atlasPath.empty()) return dialogError;
    std::filesystem::path layoutPath = atlasPath;
    layoutPath.replace_extension();
    layoutPath += "_layout.json";

    spritesheet::Result sheet;
    std::string error;
    if (!spritesheet::Build(sources, spritesheet::Options{}, sheet, &error) ||
        !formats::SavePng(sheet.atlas, atlasPath, &error) ||
        !formats::WriteWholeFile(layoutPath, sheet.layoutJson.data(), sheet.layoutJson.size(), &error)) {
        return "Export failed: " + error;
    }

    std::string status = "Exported " + std::to_string(sheet.assetCount) + " assets / " + std::to_string(sheet.frameCount) +
                         " frames (" + std::to_string(sheet.atlas.Width()) + "x" + std::to_string(sheet.atlas.Height()) +
                         ") to " + atlasPath.filename().string() + " + " + layoutPath.filename().string();
    if (notConvertedCount > 0) {
        status += " - " + std::to_string(notConvertedCount) + " asset(s) skipped (not converted yet)";
    }
    return status;
}

} // namespace pixie::services
