#include "app/views/SamplingBoxEditor.h"

#include <algorithm>
#include <cmath>

namespace pixie::views {

using graphics::CropRect;

namespace {

constexpr float kHandleHalfSize = 5.0f;
constexpr float kHandleGrabRadius = 9.0f; // bigger than the drawn square so it's easy to grab
constexpr int kMinBoxSide = 4;

bool IsInside(ImVec2 point, ImVec2 min, ImVec2 max) {
    return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
}

} // namespace

void SamplingBoxEditor::Update(graphics::CropRect& box, int imageWidth, int imageHeight, ImVec2 imageMin, ImVec2 imageSize,
                               double targetAspect) {
    if (imageWidth <= 0 || imageHeight <= 0 || box.Empty()) return;

    // Screen pixels per image pixel, on each axis.
    const ImVec2 scale(imageSize.x / static_cast<float>(imageWidth), imageSize.y / static_cast<float>(imageHeight));
    const ImVec2 imageMax(imageMin.x + imageSize.x, imageMin.y + imageSize.y);
    const ImVec2 boxMin(imageMin.x + box.x * scale.x, imageMin.y + box.y * scale.y);
    const ImVec2 boxMax(imageMin.x + box.Right() * scale.x, imageMin.y + box.Bottom() * scale.y);

    // Only corners: resizing must keep the target aspect, so single-edge handles make no sense.
    const Corner corners[4] = {
        {Handle::TopLeft, {boxMin.x, boxMin.y}},
        {Handle::TopRight, {boxMax.x, boxMin.y}},
        {Handle::BottomLeft, {boxMin.x, boxMax.y}},
        {Handle::BottomRight, {boxMax.x, boxMax.y}},
    };

    Draw(imageMin, imageMax, boxMin, boxMax, corners);
    StartDragIfClicked(box, imageMin, imageMax, boxMin, boxMax, corners);

    if (activeHandle_ == Handle::None) return;
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (activeHandle_ == Handle::Move) {
            Move(box, imageWidth, imageHeight, scale);
        } else {
            ResizeFromCorner(box, imageWidth, imageHeight, scale, targetAspect);
        }
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) activeHandle_ = Handle::None;
}

void SamplingBoxEditor::Draw(ImVec2 imageMin, ImVec2 imageMax, ImVec2 boxMin, ImVec2 boxMax, const Corner (&corners)[4]) const {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Dim the four strips outside the box.
    const ImU32 dimColor = IM_COL32(0, 0, 0, 140);
    drawList->AddRectFilled(imageMin, ImVec2(imageMax.x, boxMin.y), dimColor);
    drawList->AddRectFilled(ImVec2(imageMin.x, boxMax.y), imageMax, dimColor);
    drawList->AddRectFilled(ImVec2(imageMin.x, boxMin.y), ImVec2(boxMin.x, boxMax.y), dimColor);
    drawList->AddRectFilled(ImVec2(boxMax.x, boxMin.y), ImVec2(imageMax.x, boxMax.y), dimColor);

    drawList->AddRect(boxMin, boxMax, IM_COL32(255, 255, 255, 230), 0.0f, 0, 1.5f);
    for (const Corner& corner : corners) {
        const ImVec2 squareMin(corner.position.x - kHandleHalfSize, corner.position.y - kHandleHalfSize);
        const ImVec2 squareMax(corner.position.x + kHandleHalfSize, corner.position.y + kHandleHalfSize);
        drawList->AddRectFilled(squareMin, squareMax, IM_COL32(255, 255, 255, 255));
        drawList->AddRect(squareMin, squareMax, IM_COL32(20, 20, 20, 255));
    }
}

void SamplingBoxEditor::StartDragIfClicked(const graphics::CropRect& box, ImVec2 imageMin, ImVec2 imageMax, ImVec2 boxMin,
                                           ImVec2 boxMax, const Corner (&corners)[4]) {
    const ImVec2 mouse = ImGui::GetMousePos();
    if (activeHandle_ != Handle::None || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;
    if (!IsInside(mouse, imageMin, imageMax)) return;

    Handle grabbed = Handle::None;
    for (const Corner& corner : corners) {
        if (std::fabs(mouse.x - corner.position.x) <= kHandleGrabRadius && std::fabs(mouse.y - corner.position.y) <= kHandleGrabRadius) {
            grabbed = corner.handle;
            break;
        }
    }
    if (grabbed == Handle::None && IsInside(mouse, boxMin, boxMax)) grabbed = Handle::Move;
    if (grabbed == Handle::None) return;

    activeHandle_ = grabbed;
    boxAtDragStart_ = box;
    mouseAtDragStart_ = mouse;
}

void SamplingBoxEditor::Move(graphics::CropRect& box, int imageWidth, int imageHeight, ImVec2 scale) const {
    const ImVec2 mouse = ImGui::GetMousePos();
    const int offsetX = static_cast<int>(std::lround((mouse.x - mouseAtDragStart_.x) / scale.x));
    const int offsetY = static_cast<int>(std::lround((mouse.y - mouseAtDragStart_.y) / scale.y));
    const CropRect& start = boxAtDragStart_;
    box.x = start.x + std::clamp(offsetX, -start.x, imageWidth - start.Right());
    box.y = start.y + std::clamp(offsetY, -start.y, imageHeight - start.Bottom());
}

void SamplingBoxEditor::ResizeFromCorner(graphics::CropRect& box, int imageWidth, int imageHeight, ImVec2 scale,
                                         double targetAspect) const {
    const ImVec2 mouse = ImGui::GetMousePos();
    const CropRect& start = boxAtDragStart_;
    const bool draggingLeft = activeHandle_ == Handle::TopLeft || activeHandle_ == Handle::BottomLeft;
    const bool draggingTop = activeHandle_ == Handle::TopLeft || activeHandle_ == Handle::TopRight;

    // The opposite corner stays fixed.
    const int anchorX = draggingLeft ? start.Right() : start.x;
    const int anchorY = draggingTop ? start.Bottom() : start.y;

    // How far the mouse moved outward, in image pixels.
    const float growX = (draggingLeft ? mouseAtDragStart_.x - mouse.x : mouse.x - mouseAtDragStart_.x) / scale.x;
    const float growY = (draggingTop ? mouseAtDragStart_.y - mouse.y : mouse.y - mouseAtDragStart_.y) / scale.y;

    // Follow whichever axis moved further, then derive the other from the aspect ratio.
    const double growYAsWidth = growY * targetAspect;
    double width = start.width + (std::fabs(growX) >= std::fabs(growYAsWidth) ? growX : growYAsWidth);
    width = std::max<double>(kMinBoxSide, width);
    double height = width / targetAspect;

    // Shrink to stay inside the image.
    const double maxWidth = draggingLeft ? anchorX : imageWidth - anchorX;
    const double maxHeight = draggingTop ? anchorY : imageHeight - anchorY;
    if (width > maxWidth) {
        width = maxWidth;
        height = width / targetAspect;
    }
    if (height > maxHeight) {
        height = maxHeight;
        width = height * targetAspect;
    }

    const int finalWidth = std::max(kMinBoxSide, static_cast<int>(std::lround(width)));
    const int finalHeight = std::max(kMinBoxSide, static_cast<int>(std::lround(height)));
    box = CropRect{draggingLeft ? anchorX - finalWidth : anchorX, draggingTop ? anchorY - finalHeight : anchorY, finalWidth,
                   finalHeight};
}

} // namespace pixie::views
