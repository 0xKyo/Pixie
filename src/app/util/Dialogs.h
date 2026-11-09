#pragma once

#include <SDL3/SDL.h>

namespace pixie::util {

// Warns that `count` existing .aseprite files will be replaced; true if the user chose "Overwrite".
bool ConfirmOverwrite(SDL_Window* window, int count);

} // namespace pixie::util
