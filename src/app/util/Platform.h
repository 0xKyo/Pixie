#pragma once

#include <filesystem>

// File dialogs (nfd) take wide strings on Windows and narrow strings elsewhere.
#ifdef _WIN32
#define PIXIE_NSTR(literal) L##literal
#else
#define PIXIE_NSTR(literal) literal
#endif

namespace pixie::util {

// The OS's native path string type (std::wstring on Windows).
using NativeString = std::filesystem::path::string_type;

} // namespace pixie::util
