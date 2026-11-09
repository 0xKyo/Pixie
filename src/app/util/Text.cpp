#include "app/util/Text.h"

#include <chrono>
#include <cstdio>
#include <ctime>

namespace pixie::util {

std::string FormatBytes(std::uintmax_t bytes) {
    char text[32];
    if (bytes >= 1024 * 1024) {
        std::snprintf(text, sizeof(text), "%.2f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    } else if (bytes >= 1024) {
        std::snprintf(text, sizeof(text), "%.1f KB", static_cast<double>(bytes) / 1024.0);
    } else {
        std::snprintf(text, sizeof(text), "%llu B", static_cast<unsigned long long>(bytes));
    }
    return text;
}

std::string FormatModified(const std::filesystem::path& path) {
    std::error_code error;
    const auto fileTime = std::filesystem::last_write_time(path, error);
    if (error) return "?";
    const auto systemTime = std::chrono::clock_cast<std::chrono::system_clock>(fileTime);
    const std::time_t seconds = std::chrono::system_clock::to_time_t(systemTime);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &seconds);
#else
    localtime_r(&seconds, &localTime);
#endif
    char text[32];
    std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M", &localTime);
    return text;
}

std::string FormatDuration(double seconds) {
    const int wholeSeconds = static_cast<int>(seconds);
    const int tenths = static_cast<int>((seconds - wholeSeconds) * 10);
    char text[32];
    std::snprintf(text, sizeof(text), "%d:%02d.%d", wholeSeconds / 60, wholeSeconds % 60, tenths);
    return text;
}

std::uintmax_t FileSize(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    return error ? 0 : size;
}

std::filesystem::path Utf8Path(const std::string& utf8) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

std::string ToLower(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    }
    return text;
}

} // namespace pixie::util
