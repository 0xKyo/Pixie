#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif

#include "app/services/ExternalTools.h"

#include <SDL3/SDL.h>

#include <nfd.hpp>

#include <cstdlib>
#include <vector>

#include "app/util/Platform.h"
#include "app/util/Text.h"

namespace pixie::services {

namespace {

#ifdef _WIN32
// Folders where Windows programs are usually installed.
std::vector<std::filesystem::path> ProgramFolders() {
    std::vector<std::filesystem::path> folders;
    for (const char* variable : {"ProgramFiles", "ProgramFiles(x86)", "LOCALAPPDATA"}) {
        if (const char* value = std::getenv(variable)) {
            folders.emplace_back(value);
            if (std::string(variable) == "LOCALAPPDATA") folders.push_back(std::filesystem::path(value) / "Programs");
        }
    }
    return folders;
}
#endif

// Looks for audacity*.exe up to two levels inside any "Audacity*" install folder.
std::filesystem::path FindAudacity() {
#ifdef _WIN32
    for (const std::filesystem::path& programs : ProgramFolders()) {
        std::error_code error;
        for (std::filesystem::directory_iterator install(programs, error), end; !error && install != end; install.increment(error)) {
            if (!install->is_directory(error)) continue;
            if (util::ToLower(install->path().filename().string()).rfind("audacity", 0) != 0) continue;

            std::filesystem::recursive_directory_iterator file(install->path(), std::filesystem::directory_options::skip_permission_denied, error);
            for (const std::filesystem::recursive_directory_iterator fileEnd; !error && file != fileEnd; file.increment(error)) {
                if (file.depth() > 2) {
                    file.disable_recursion_pending();
                    continue;
                }
                const std::filesystem::path& candidate = file->path();
                const bool isExe = util::ToLower(candidate.extension().string()) == ".exe";
                if (isExe && util::ToLower(candidate.stem().string()).rfind("audacity", 0) == 0) return candidate;
            }
        }
    }
#endif
    return {};
}

// Asks the user to point at the Audacity executable; empty if they cancel.
std::filesystem::path AskForAudacity() {
    NFD::UniquePathN picked;
#ifdef _WIN32
    const nfdnfilteritem_t filters[] = {{PIXIE_NSTR("Executable"), PIXIE_NSTR("exe")}};
    const nfdresult_t result = NFD::OpenDialog(picked, filters, 1);
#else
    const nfdresult_t result = NFD::OpenDialog(picked, nullptr, 0);
#endif
    return result == NFD_OKAY ? std::filesystem::path(picked.get()) : std::filesystem::path();
}

} // namespace

std::string OpenInAseprite(const std::filesystem::path& file) {
#ifdef _WIN32
    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", file.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32) { // ShellExecute reports errors as values <= 32
        return "Could not open " + file.filename().string() + " (is Aseprite installed and associated with .aseprite?)";
    }
#else
    const std::string url = "file://" + file.generic_string();
    if (!SDL_OpenURL(url.c_str())) return std::string("Could not open file: ") + SDL_GetError();
#endif
    return {};
}

std::string OpenInAudacity(const std::filesystem::path& file, Settings& settings) {
    std::error_code error;
    std::filesystem::path audacity = settings.audacityPath;
    if (audacity.empty() || !std::filesystem::exists(audacity, error)) {
        audacity = FindAudacity();
        if (audacity.empty()) audacity = AskForAudacity();
        if (audacity.empty()) return "Audacity wasn't found - pick its executable to use \"Open in Audacity\"";
        settings.audacityPath = audacity;
        settings.Save();
    }
#ifdef _WIN32
    const std::wstring arguments = L"\"" + file.wstring() + L"\"";
    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", audacity.c_str(), arguments.c_str(), nullptr, SW_SHOWNORMAL));
    if (result <= 32) {
        settings.audacityPath.clear(); // forget a broken path so the next attempt searches again
        return "Could not start Audacity (" + audacity.string() + ")";
    }
#else
    const std::string command = "\"" + audacity.string() + "\" \"" + file.string() + "\" &";
    if (std::system(command.c_str()) != 0) return "Could not start Audacity";
#endif
    return {};
}

} // namespace pixie::services
