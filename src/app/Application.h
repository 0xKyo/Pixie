#pragma once

#include <SDL3/SDL.h>

#include <filesystem>
#include <future>
#include <string>

#include "core/Image.h"
#include "core/Palette.h"

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

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool imguiInitialized_ = false;
    bool nfdInitialized_ = false;

    Image sourceImage_;
    Image resultImage_;
    Palette palette_;

    SDL_Texture* sourceTexture_ = nullptr;
    SDL_Texture* resultTexture_ = nullptr;
    // Snapshot of resultImage_'s size, updated only once a conversion has
    // finished. RenderUi() must not read resultImage_ directly while
    // `converting_` is true, since a background thread may be writing to it.
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
