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
    void RenderCropOverlay(ImDrawList* drawList, float itemMinX, float itemMinY, float itemW, float itemH);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool imguiInitialized_ = false;
    bool nfdInitialized_ = false;

    Image sourceImage_;
    Image resultImage_;
    Palette palette_;

    // Only mutated by the UI thread while `converting_` is false; DoConvert()
    // bakes it into a working copy before handing off to the worker thread.
    Mask backgroundMask_;
    bool removeBackgroundEnabled_ = false;
    float backgroundTolerance_ = 10.0f; // CIEDE2000 units; slider range [1, 30]

    // Crop tool state, in source-image pixel coordinates. Only mutated by
    // the UI thread, same discipline as backgroundMask_ above.
    CropRect cropRect_;
    bool cropEnabled_ = false;
    float cropTolerance_ = 10.0f; // CIEDE2000 units; slider range [1, 30], used by Auto Crop only

    enum class CropHandle { None, Move, N, S, E, W, NE, NW, SE, SW };
    CropHandle activeCropHandle_ = CropHandle::None;
    CropRect cropDragStartRect_;
    float cropDragStartMouseX_ = 0.0f;
    float cropDragStartMouseY_ = 0.0f;

    // Downscale tool state. Only mutated by the UI thread; DoConvert()
    // snapshots these before handing conversion off to the worker thread.
    bool downscaleEnabled_ = false;
    int downscaleWidth_ = 64; // one of {16, 32, 64, 128}
    // true (default) = quantize at full res before downscaling. false =
    // downscale first, then quantize -- exposed so both orders can be compared.
    bool quantizeBeforeDownscale_ = true;

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
