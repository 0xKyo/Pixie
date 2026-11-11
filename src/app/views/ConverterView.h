#pragma once

#include <SDL3/SDL.h>

#include <filesystem>
#include <future>
#include <string>
#include <vector>

#include "app/AppContext.h"
#include "app/services/AnimationImport.h"
#include "app/services/BatchConversion.h"
#include "app/services/ConversionPipeline.h"
#include "app/views/SamplingBoxEditor.h"
#include "core/color/Palette.h"
#include "core/graphics/Crop.h"
#include "core/graphics/Image.h"
#include "core/graphics/Mask.h"

namespace pixie::views {

// The conversion screen: remove background, downsize and remap images to a palette, one or many at a time.
class ConverterView {
public:
    explicit ConverterView(app::AppContext& context) : context_(context) {}
    ~ConverterView() { Shutdown(); }
    ConverterView(const ConverterView&) = delete;
    ConverterView& operator=(const ConverterView&) = delete;

    void LoadStartupPalette();
    void Render();
    void Update();   // call every frame: collects finished background conversions
    void Shutdown(); // waits for the worker and frees textures; call before the renderer is destroyed

    // Opens one asset: its base image on the left, its existing .aseprite (if any) on the right.
    void OpenAsset(const library::Asset& asset);
    // Opens several images as a batch whose results are saved next to each original.
    void OpenBatch(std::vector<std::filesystem::path> paths, size_t skippedWithoutBase);
    // Asks for a folder and opens its images as the frames of one animation (saved together as one .aseprite).
    void ImportAnimationFromDialog();

private:
    // The sidebar's tool switches.
    struct ToolSettings {
        bool usePalette = true;
        bool removeTransparency = false;
        bool removeBackground = false;
        float backgroundTolerance = 10.0f; // CIEDE2000 units, slider range 1..30
        bool downsize = false;
        int targetWidth = 320;
        int targetHeight = 180;
        bool keepAspect = true; // sample a box with the target's aspect instead of stretching
    };

    // Toolbar actions.
    void OpenImagesFromDialog();
    void OpenPaletteFromDialog();
    void SaveResultFromDialog();
    void SaveResultBesideOriginal();
    void ConvertPreviewedImage();
    void ConvertAllImages();
    void ConvertAnimation();

    // Layout.
    void RenderToolbar();
    void RenderSidebar();
    void RenderPalettePanel();
    void RenderBatchPanel();
    void RenderAnimationOptions();
    void RenderToolsPanel();
    void RenderDownsizeTool();
    void RenderSourcePreview(float width);
    void RenderResultPreview();

    // State helpers.
    bool LoadSourceImage(const std::filesystem::path& path); // false (and nothing changed) if it can't be read
    void SelectBatchImage(size_t index);
    void ShowResult(graphics::Image image);
    void RebuildSourceTexture();
    void RecomputeBackgroundMask();
    void ResetSamplingBox();
    void FinishConversion();
    [[nodiscard]] services::BatchJob BuildBatchJob() const; // tool settings and sources shared by batches and animations
    [[nodiscard]] services::ConversionSettings CurrentSettings() const;
    [[nodiscard]] bool IsSamplingBoxActive() const { return tools_.downsize && tools_.keepAspect; }
    [[nodiscard]] bool HasResult() const { return resultWidth_ > 0; }

    app::AppContext& context_;

    graphics::Image source_;
    std::filesystem::path sourcePath_;
    SDL_Texture* sourceTexture_ = nullptr;

    graphics::Image result_; // written by the worker while `converting_`; read resultWidth_/Height_ instead then
    SDL_Texture* resultTexture_ = nullptr;
    int resultWidth_ = 0;
    int resultHeight_ = 0;

    color::Palette palette_;
    ToolSettings tools_;
    graphics::Mask backgroundMask_;
    graphics::CropRect samplingBox_; // in source pixels; only used while IsSamplingBoxActive()
    SamplingBoxEditor samplingBoxEditor_;

    std::vector<std::filesystem::path> batchPaths_; // every image opened; one of them is previewed
    size_t previewedIndex_ = 0;
    char batchBaseName_[128] = "converted";         // UTF-8, for <name>_01, <name>_02...
    bool batchKeepNames_ = false;
    bool batchBesideOriginals_ = false;             // write <original>.aseprite next to each source
    bool saveAsAseprite_ = false;
    bool animationMode_ = false;              // the images are the frames of one animation, in order
    std::string animationName_ = "animation"; // UTF-8, the suggested file name
    int animationFrameDurationMs_ = 100;

    std::future<void> worker_;
    bool converting_ = false;
    bool convertingBatch_ = false;
    bool convertingAnimation_ = false;
    std::filesystem::path animationOutputPath_;
    int batchTotal_ = 0;
    std::filesystem::path batchOutputFolder_;
    services::BatchProgress batchProgress_;
};

} // namespace pixie::views
