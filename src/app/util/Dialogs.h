#pragma once

#include <SDL3/SDL.h>

#include <string>

namespace pixie::util {

// A warning box with Cancel and a confirm button; true if the user chose the confirm button.
bool Confirm(SDL_Window* window, const std::string& message, const char* confirmLabel);

// Warns that `count` existing .aseprite files will be replaced; true if the user chose "Overwrite".
bool ConfirmOverwrite(SDL_Window* window, int count);

} // namespace pixie::util
