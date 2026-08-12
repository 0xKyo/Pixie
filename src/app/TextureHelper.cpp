#include "app/TextureHelper.h"

namespace pixie {

SDL_Texture* CreateTextureFromImage(SDL_Renderer* renderer, const Image& image) {
    if (image.Empty()) {
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                              image.Width(), image.Height());
    if (!texture) {
        return nullptr;
    }

    // Pixel art should never be blurred when scaled.
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    // Required for transparent (alpha=0) pixels to actually preview as
    // transparent rather than opaque.
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    const int pitch = image.Width() * static_cast<int>(sizeof(RGB8));
    if (!SDL_UpdateTexture(texture, nullptr, image.Pixels(), pitch)) {
        SDL_DestroyTexture(texture);
        return nullptr;
    }

    return texture;
}

} // namespace pixie
