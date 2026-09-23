#pragma once

#include <SDL3/SDL.h>

#include <filesystem>
#include <future>
#include <string>

#include "core/Crop.h"
#include "core/Image.h"
#include "core/Mask.h"
#include "core/Palette.h"

struct ImDrawList; // avoid pulling imgui.h into this header just for a pointer param

namespace pixie {

class Application {
public:
    Application() = default;
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Creates the window, renderer, Dear ImGui context and file-dialog backend.
    // Returns false on failure; call LastError() for details.
    bool Init();

    // Runs the main loop until the window is closed. Returns a process exit code.
    int Run();

    [[nodiscard]] const std::string& LastError() const { return lastError_; }

private:
    void Shutdown();
    void RenderUi();
    void PollConversion();

    void DoOpenImage();
    void DoOpenPalette();
    void DoSaveResult();
    void DoConvert();

    void RebuildSourceTexture();
    void RebuildResultTexture();
    void RecomputeBackgroundMask();
    void RenderDownsizeOverlay(ImDrawList* drawList, float itemMinX, float itemMinY, float itemW, float itemH);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool imguiInitialized_ = false;
    bool nfdInitialized_ = false;

    Image sourceImage_;
    Image resultImage_;
    Palette palette_;
    // When off, Convert keeps the original colors and only applies the other tools.
    bool usePaletteEnabled_ = true;

    // Only mutated by the UI thread while `converting_` is false; DoConvert()
    // bakes it into a working copy before handing off to the worker thread.
    Mask backgroundMask_;
    bool removeBackgroundEnabled_ = false;
    float backgroundTolerance_ = 10.0f; // CIEDE2000 units; slider range [1, 30]

    // Forces any partially transparent pixel (alpha > 0) to fully opaque before conversion.
    bool removeTransparencyEnabled_ = false;

    // Downsize tool state: free-form target size with an optional
    // aspect-locked sampling rect. Internal names use "Downsize" to avoid
    // clashing with the Remove Background tool above. Only mutated by the
    // UI thread; DoConvert() snapshots these before handing conversion off
    // to the worker thread. The palette is always swapped before resizing.
    bool downsizeEnabled_ = false;
    int downsizeTargetWidth_ = 320;
    int downsizeTargetHeight_ = 180;
    // Defaults on: the sampling box shows immediately once Downsize is
    // enabled, so resizing always preserves the target aspect ratio unless
    // explicitly unchecked to allow a non-uniform stretch.
    bool downsizeMaintainRatio_ = true;
    // Sampling region, in source-image pixel coordinates, that gets scaled
    // to fill the target size. Only meaningful while downsizeMaintainRatio_
    // is true; only mutated by the UI thread.
    CropRect downsizeCropRect_;

    enum class DownsizeHandle { None, Move, NW, NE, SW, SE };
    DownsizeHandle activeDownsizeHandle_ = DownsizeHandle::None;
    CropRect downsizeDragStartRect_;
    float downsizeDragStartMouseX_ = 0.0f;
    float downsizeDragStartMouseY_ = 0.0f;

    SDL_Texture* sourceTexture_ = nullptr;
    SDL_Texture* resultTexture_ = nullptr;
    // Snapshot of resultImage_'s size, set once conversion finishes;
    // RenderUi() must not read resultImage_ directly while `converting_` is true.
    int resultWidth_ = 0;
    int resultHeight_ = 0;

    std::filesystem::path sourcePath_;

    std::future<void> conversionFuture_;
    bool converting_ = false;

    std::string statusMessage_;
    std::string lastError_;

    bool running_ = false;
};

} // namespace pixie
