#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "app/services/ProjectStore.h"
#include "core/library/Asset.h"

namespace pixie::services {

// Asks for a folder, writes the art as atlas.png + atlas.json and copies the audio into SFX/; returns a status line.
std::string ExportProject(const Project& project, const std::vector<library::Asset>& assets,
                          const std::filesystem::path& libraryRoot);

} // namespace pixie::services
