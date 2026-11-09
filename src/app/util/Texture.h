#pragma once

#include <SDL3/SDL.h>

#include "core/graphics/Image.h"

namespace pixie::util {

// Uploads `image` as a GPU texture with nearest-neighbor sampling; nullptr on failure or empty image.
SDL_Texture* CreateTexture(SDL_Renderer* renderer, const graphics::Image& image);

// Destroys the texture (if any) and resets the pointer.
void DestroyTexture(SDL_Texture*& texture);

} // namespace pixie::util
