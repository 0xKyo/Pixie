#pragma once

#include "core/color/Palette.h"
#include "core/graphics/Image.h"

namespace pixie::graphics {

// Replaces every pixel with its perceptually closest palette color (CIEDE2000), keeping alpha.
void RemapToPalette(const Image& source, const color::Palette& palette, Image& result);

// Snaps alpha to fully opaque or fully transparent, whichever is closer.
void BinarizeAlpha(Image& image);

// Makes every partially transparent pixel fully opaque; fully transparent pixels stay transparent.
void RemoveTransparency(Image& image);

} // namespace pixie::graphics
