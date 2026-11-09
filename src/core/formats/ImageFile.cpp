#include "core/formats/ImageFile.h"

// stb's implementation is compiled here; Aseprite.cpp also uses its zlib decoder.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cstring>
#include <vector>

#include "core/formats/Aseprite.h"
#include "core/formats/BinaryFile.h"

namespace pixie::formats {

bool LoadImageFile(const std::filesystem::path& path, graphics::Image& image, std::string* error) {
    std::vector<unsigned char> bytes;
    if (!ReadWholeFile(path, bytes, error)) return false;

    if (IsAsepritePath(path)) {
        return DecodeAsepriteFirstFrame(bytes.data(), bytes.size(), image, error);
    }

    int width = 0, height = 0, channelsInFile = 0;
    stbi_uc* pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channelsInFile, 4);
    if (!pixels) {
        if (error) *error = stbi_failure_reason() ? stbi_failure_reason() : "Unknown image decoding error";
        return false;
    }
    image.Resize(width, height);
    std::memcpy(image.MutablePixels(), pixels, static_cast<size_t>(width) * height * 4);
    stbi_image_free(pixels);
    return true;
}

bool SavePng(const graphics::Image& image, const std::filesystem::path& path, std::string* error) {
    if (image.Empty()) {
        if (error) *error = "Cannot save an empty image";
        return false;
    }
    // stb writes into a callback, which collects the encoded bytes in memory.
    std::vector<unsigned char> encoded;
    auto appendBytes = [](void* context, void* data, int size) {
        auto* buffer = static_cast<std::vector<unsigned char>*>(context);
        const auto* bytes = static_cast<const unsigned char*>(data);
        buffer->insert(buffer->end(), bytes, bytes + size);
    };
    const int rowBytes = image.Width() * 4;
    if (!stbi_write_png_to_func(appendBytes, &encoded, image.Width(), image.Height(), 4, image.Pixels(), rowBytes)) {
        if (error) *error = "Failed to encode PNG: " + path.string();
        return false;
    }
    return WriteWholeFile(path, encoded.data(), encoded.size(), error);
}

bool SaveAseprite(const graphics::Image& image, const std::filesystem::path& path, std::string* error) {
    std::vector<unsigned char> encoded;
    if (!EncodeAseprite(image, encoded, error)) return false;
    return WriteWholeFile(path, encoded.data(), encoded.size(), error);
}

} // namespace pixie::formats
