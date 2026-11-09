#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "app/services/ProjectStore.h"
#include "core/library/Asset.h"

namespace pixie::services {

// Asks where to save, writes <name>.png + <name>_layout.json, and returns a status line ("" if cancelled).
std::string ExportSpriteSheet(const Project& project, const std::vector<library::Asset>& assets,
                              const std::filesystem::path& libraryRoot);

} // namespace pixie::services
