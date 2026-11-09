#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace pixie::util {

[[nodiscard]] std::string FormatBytes(std::uintmax_t bytes);                 // e.g. "1.5 KB"
[[nodiscard]] std::string FormatModified(const std::filesystem::path& path); // local "YYYY-MM-DD HH:MM"
[[nodiscard]] std::string FormatDuration(double seconds);                    // "m:ss.t"

[[nodiscard]] std::uintmax_t FileSize(const std::filesystem::path& path); // 0 if unreadable
[[nodiscard]] std::filesystem::path Utf8Path(const std::string& utf8);
[[nodiscard]] std::string ToLower(std::string text); // ASCII only

} // namespace pixie::util
