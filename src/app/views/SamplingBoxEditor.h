#pragma once

#include <imgui.h>

#include "core/graphics/Crop.h"

namespace pixie::views {

// The Downsize "sampling box": drawn over the source preview, moved by dragging and resized by its corners.
class SamplingBoxEditor {
public:
    // Draws `box` over an image shown at `imageMin`/`imageSize` on screen and applies any mouse drag to it.
    void Update(graphics::CropRect& box, int imageWidth, int imageHeight, ImVec2 imageMin, ImVec2 imageSize,
                double targetAspect);

    void CancelDrag() { activeHandle_ = Handle::None; }

private:
    enum class Handle { None, Move, TopLeft, TopRight, BottomLeft, BottomRight };

    struct Corner {
        Handle handle;
        ImVec2 position;
    };

    void Draw(ImVec2 imageMin, ImVec2 imageMax, ImVec2 boxMin, ImVec2 boxMax, const Corner (&corners)[4]) const;
    void StartDragIfClicked(const graphics::CropRect& box, ImVec2 imageMin, ImVec2 imageMax, ImVec2 boxMin, ImVec2 boxMax,
                            const Corner (&corners)[4]);
    void Move(graphics::CropRect& box, int imageWidth, int imageHeight, ImVec2 pixelsPerImagePixel) const;
    void ResizeFromCorner(graphics::CropRect& box, int imageWidth, int imageHeight, ImVec2 pixelsPerImagePixel,
                          double targetAspect) const;

    Handle activeHandle_ = Handle::None;
    graphics::CropRect boxAtDragStart_;
    ImVec2 mouseAtDragStart_;
};

} // namespace pixie::views
