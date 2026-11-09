#pragma once

#include <filesystem>
#include <string>

#include "app/services/Settings.h"

namespace pixie::services {

// Both return "" on success, or a message to show the user.

// Opens the file with the app the OS associates with it (Aseprite, for .aseprite files).
std::string OpenInAseprite(const std::filesystem::path& file);

// Opens the file in Audacity, finding it automatically or asking once and saving the choice in `settings`.
std::string OpenInAudacity(const std::filesystem::path& file, Settings& settings);

} // namespace pixie::services
