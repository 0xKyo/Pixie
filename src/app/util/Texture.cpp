#include "app/util/Texture.h"

namespace pixie::util {

SDL_Texture* CreateTexture(SDL_Renderer* renderer, const graphics::Image& image) {
    if (image.Empty()) return nullptr;

    SDL_Texture* texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image.Width(), image.Height());
    if (!texture) return nullptr;

    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST); // pixel art must stay crisp when scaled
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);   // so alpha 0 actually shows as transparent

    const int rowBytes = image.Width() * 4;
    if (!SDL_UpdateTexture(texture, nullptr, image.Pixels(), rowBytes)) {
        SDL_DestroyTexture(texture);
        return nullptr;
    }
    return texture;
}

void DestroyTexture(SDL_Texture*& texture) {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
}

} // namespace pixie::util
