#include "app/util/Gui.h"

#include <algorithm>
#include <cmath>

namespace pixie::util {

void BeginFullscreenWindow(const char* id) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin(id, nullptr, kFlags);
}

ImVec2 FitInside(int width, int height, ImVec2 available) {
    if (width <= 0 || height <= 0 || available.x <= 1.0f || available.y <= 1.0f) return ImVec2(0.0f, 0.0f);
    const float scale = std::min(available.x / static_cast<float>(width), available.y / static_cast<float>(height));
    return ImVec2(width * scale, height * scale);
}

ImVec2 FitInsidePixelPerfect(int width, int height, ImVec2 available) {
    if (width <= 0 || height <= 0) return ImVec2(0.0f, 0.0f);
    float scale = std::min(available.x / static_cast<float>(width), available.y / static_cast<float>(height));
    if (scale >= 1.0f) scale = std::floor(scale);
    return ImVec2(width * scale, height * scale);
}

} // namespace pixie::util
