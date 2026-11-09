#include "core/graphics/Image.h"

namespace pixie::graphics {

static_assert(sizeof(color::Rgba) == 4, "Rgba must be 4 bytes so Pixels() is a packed RGBA8 buffer");

void Image::Resize(int width, int height, color::Rgba fill) {
    width_ = width;
    height_ = height;
    pixels_.assign(static_cast<size_t>(width) * static_cast<size_t>(height), fill);
}

void Image::Clear() {
    width_ = 0;
    height_ = 0;
    pixels_.clear();
}

} // namespace pixie::graphics
