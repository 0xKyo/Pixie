#include "app/services/ConversionPipeline.h"

#include "core/graphics/PaletteRemap.h"
#include "core/graphics/Resize.h"

namespace pixie::services {

graphics::Image ConvertImage(const graphics::Image& source, const ConversionSettings& settings,
                             const graphics::Mask* backgroundMask, const graphics::CropRect& crop,
                             const color::Palette* palette) {
    const bool cropFirst = !crop.Empty();

    graphics::Image result;
    if (cropFirst) {
        graphics::CropImage(source, crop, result);
    } else {
        result = source;
    }

    // Before the mask, so the background it clears stays fully transparent.
    if (settings.removeTransparency) {
        graphics::RemoveTransparency(result);
    }

    // The mask matches the uncropped source, so it is cropped the same way.
    if (backgroundMask && backgroundMask->MatchesSize(source.Width(), source.Height())) {
        if (cropFirst) {
            graphics::Mask croppedMask;
            graphics::CropMask(*backgroundMask, crop, croppedMask);
            graphics::ApplyMask(croppedMask, result);
        } else {
            graphics::ApplyMask(*backgroundMask, result);
        }
    }

    // Remap before resizing so the palette match sees full-resolution detail.
    if (palette && !palette->Empty()) {
        graphics::Image remapped;
        graphics::RemapToPalette(result, *palette, remapped);
        result = std::move(remapped);
    }

    // Nearest-neighbor never invents a color outside the palette we just applied.
    if (settings.resize) {
        graphics::Image resized;
        graphics::ResizeNearest(result, settings.targetWidth, settings.targetHeight, resized);
        result = std::move(resized);
    }

    graphics::BinarizeAlpha(result); // masking and resizing can leave partial alpha
    return result;
}

} // namespace pixie::services
