#include "core/formats/BinaryFile.h"

#include <fstream>
#include <iterator>

namespace pixie::formats {

bool ReadWholeFile(const std::filesystem::path& path, std::vector<unsigned char>& bytes, std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error) *error = "Could not open file: " + path.string();
        return false;
    }
    bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    if (bytes.empty()) {
        if (error) *error = "Empty or unreadable file: " + path.string();
        return false;
    }
    return true;
}

bool WriteWholeFile(const std::filesystem::path& path, const void* data, size_t size, std::string* error) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file || !file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size))) {
        if (error) *error = "Could not write file: " + path.string();
        return false;
    }
    return true;
}

} // namespace pixie::formats
