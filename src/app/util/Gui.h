#pragma once

#include <imgui.h>

#include <SDL3/SDL.h>

namespace pixie::util {

// Begins an undecorated ImGui window covering the whole app window; pair with ImGui::End().
void BeginFullscreenWindow(const char* id);

// Size that fits `width` x `height` inside `available`, keeping the aspect ratio.
ImVec2 FitInside(int width, int height, ImVec2 available);

// Like FitInside, but enlarges by whole multiples so pixel art stays sharp.
ImVec2 FitInsidePixelPerfect(int width, int height, ImVec2 available);

// An SDL texture in the form ImGui expects.
inline ImTextureID ToImGui(SDL_Texture* texture) { return static_cast<ImTextureID>(reinterpret_cast<intptr_t>(texture)); }

} // namespace pixie::util
