#pragma once

#include <SDL3/SDL.h>

#include "core/Image.h"

namespace pixie {

// Creates a new static SDL_Texture initialized with the contents of `image`,
// using nearest-neighbor sampling (appropriate for pixel art). Returns
// nullptr on failure (check SDL_GetError()) or if `image` is empty.
SDL_Texture* CreateTextureFromImage(SDL_Renderer* renderer, const Image& image);

} // namespace pixie
