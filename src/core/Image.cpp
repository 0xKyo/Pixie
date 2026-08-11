#include "core/Image.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cstring>
#include <fstream>
#include <iterator>

namespace pixie {

static_assert(sizeof(RGB8) == 4, "RGB8 must be exactly 4 bytes to alias a tightly packed RGBA8 buffer");

void Image::Resize(int width, int height) {
    width_ = width;
    height_ = height;
    pixels_.assign(static_cast<size_t>(width) * static_cast<size_t>(height), RGB8{});
}

void Image::Clear() {
    width_ = 0;
    height_ = 0;
    pixels_.clear();
}

bool Image::LoadFromFile(const std::filesystem::path& path, std::string* error) {
    // Read the file ourselves (rather than letting stb_image fopen() the path)
    // so that non-ASCII paths work correctly on Windows.
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error) *error = "Could not open file: " + path.string();
        return false;
    }
    std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (buffer.empty()) {
        if (error) *error = "Empty or unreadable file: " + path.string();
        return false;
    }

    int w = 0, h = 0, channelsInFile = 0;
    stbi_uc* data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &w, &h, &channelsInFile, 4);
    if (!data) {
        if (error) *error = stbi_failure_reason() ? stbi_failure_reason() : "Unknown image decoding error";
        return false;
    }

    width_ = w;
    height_ = h;
    pixels_.resize(static_cast<size_t>(w) * static_cast<size_t>(h));
    std::memcpy(pixels_.data(), data, pixels_.size() * sizeof(RGB8));
    stbi_image_free(data);
    return true;
}

bool Image::SaveToPng(const std::filesystem::path& path, std::string* error) const {
    if (Empty()) {
        if (error) *error = "Cannot save an empty image";
        return false;
    }

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        if (error) *error = "Could not open file for writing: " + path.string();
        return false;
    }

    struct WriteCtx {
        std::ofstream* out;
        bool failed = false;
    } ctx{&file};

    auto writeFunc = [](void* context, void* data, int size) {
        auto* c = static_cast<WriteCtx*>(context);
        if (!c->out->write(static_cast<const char*>(data), size)) {
            c->failed = true;
        }
    };

    const int stride = width_ * static_cast<int>(sizeof(RGB8));
    const int ok = stbi_write_png_to_func(writeFunc, &ctx, width_, height_, 4, pixels_.data(), stride);
    if (!ok || ctx.failed || !file) {
        if (error) *error = "Failed to encode/write PNG: " + path.string();
        return false;
    }
    return true;
}

} // namespace pixie
