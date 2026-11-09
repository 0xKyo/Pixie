#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace pixie::formats {

// Reads or writes a whole file through std::filesystem::path, so non-ASCII paths work on Windows.
bool ReadWholeFile(const std::filesystem::path& path, std::vector<unsigned char>& bytes, std::string* error = nullptr);
bool WriteWholeFile(const std::filesystem::path& path, const void* data, size_t size, std::string* error = nullptr);

} // namespace pixie::formats
