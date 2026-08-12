#include "app/Application.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <nfd.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>

#include "app/TextureHelper.h"
#include "core/Converter.h"
#include "core/Crop.h"
#include "core/Downscale.h"
#include "core/Mask.h"

#include <cmath>
#include <memory>

namespace pixie {

namespace {

#ifdef _WIN32
#define PIXIE_NSTR(literal) L##literal
#else
#define PIXIE_NSTR(literal) literal
#endif

using NativeString = std::filesystem::path::string_type;

// Default starting directories for the file dialogs.
constexpr const nfdnchar_t* kDefaultSourceDir = PIXIE_NSTR("E:\\Workspace\\Pixie\\assets\\source");
constexpr const nfdnchar_t* kDefaultPaletteDir = PIXIE_NSTR("E:\\Workspace\\Pixie\\assets\\palettes");
constexpr const nfdnchar_t* kDefaultConvertedDir = PIXIE_NSTR("E:\\Workspace\\Pixie\\assets\\converted");

ImVec2 FitToRegion(int imageWidth, int imageHeight, ImVec2 avail) {
    if (imageWidth <= 0 || imageHeight <= 0 || avail.x <= 1.0f || avail.y <= 1.0f) {
        return ImVec2(0.0f, 0.0f);
    }
    const float scale = std::min(avail.x / static_cast<float>(imageWidth), avail.y / static_cast<float>(imageHeight));
    return ImVec2(imageWidth * scale, imageHeight * scale);
}

} // namespace

Application::~Application() {
    if (window_ || renderer_) {
        Shutdown();
    }
}

bool Application::Init() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        lastError_ = SDL_GetError();
        return false;
    }

    const SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    window_ = SDL_CreateWindow("Pixie - Pixel Art Image Converter", 1280, 800, windowFlags);
    if (!window_) {
        lastError_ = SDL_GetError();
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_) {
        lastError_ = SDL_GetError();
        return false;
    }
    SDL_SetRenderVSync(renderer_, 1);
    SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window_);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_);
    ImGui_ImplSDLRenderer3_Init(renderer_);
    imguiInitialized_ = true;

    if (NFD::Init() != NFD_OKAY) {
        lastError_ = "Failed to initialize the native file dialog library";
        return false;
    }
    nfdInitialized_ = true;

    return true;
}

void Application::Shutdown() {
    if (conversionFuture_.valid()) {
        conversionFuture_.wait();
    }

    if (sourceTexture_) {
        SDL_DestroyTexture(sourceTexture_);
        sourceTexture_ = nullptr;
    }
    if (resultTexture_) {
        SDL_DestroyTexture(resultTexture_);
        resultTexture_ = nullptr;
    }

    if (nfdInitialized_) {
        NFD::Quit();
        nfdInitialized_ = false;
    }

    if (imguiInitialized_) {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        imguiInitialized_ = false;
    }

    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
}

int Application::Run() {
    running_ = true;
    while (running_) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) {
                running_ = false;
            }
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window_)) {
                running_ = false;
            }
        }

        if (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

        PollConversion();

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        RenderUi();

        ImGui::Render();
        SDL_SetRenderDrawColor(renderer_, 30, 30, 34, 255);
        SDL_RenderClear(renderer_);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
        SDL_RenderPresent(renderer_);
    }

    Shutdown();
    return 0;
}

void Application::PollConversion() {
    if (!converting_ || !conversionFuture_.valid()) {
        return;
    }
    if (conversionFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        return;
    }
    conversionFuture_.get();
    converting_ = false;
    resultWidth_ = resultImage_.Width();
    resultHeight_ = resultImage_.Height();
    RebuildResultTexture();
    statusMessage_ = "Converted to " + std::to_string(resultWidth_) + "x" + std::to_string(resultHeight_);
    if (!palette_.Empty()) {
        statusMessage_ += " using palette \"" + palette_.Name() + "\"";
    }
    if (downscaleEnabled_ && !palette_.Empty()) {
        statusMessage_ += quantizeBeforeDownscale_ ? " (palette swapped before downscale)"
                                                     : " (palette swapped after downscale)";
    }
}

void Application::RebuildSourceTexture() {
    if (sourceTexture_) {
        SDL_DestroyTexture(sourceTexture_);
        sourceTexture_ = nullptr;
    }
    if (removeBackgroundEnabled_ && backgroundMask_.MatchesSize(sourceImage_.Width(), sourceImage_.Height())) {
        Image masked = sourceImage_; // cheap copy for preview only; no CIEDE2000 involved
        ApplyBackgroundMask(backgroundMask_, masked);
        sourceTexture_ = CreateTextureFromImage(renderer_, masked);
    } else {
        sourceTexture_ = CreateTextureFromImage(renderer_, sourceImage_);
    }
}

void Application::RecomputeBackgroundMask() {
    if (sourceImage_.Empty()) {
        backgroundMask_.Clear();
        return;
    }
    ComputeBackgroundMask(sourceImage_, static_cast<double>(backgroundTolerance_), backgroundMask_);
}

void Application::RenderCropOverlay(ImDrawList* drawList, float itemMinX, float itemMinY, float itemW, float itemH) {
    const int imgW = sourceImage_.Width();
    const int imgH = sourceImage_.Height();
    if (imgW <= 0 || imgH <= 0) {
        return;
    }
    const float scaleX = itemW / static_cast<float>(imgW);
    const float scaleY = itemH / static_cast<float>(imgH);

    const ImVec2 itemMin(itemMinX, itemMinY);
    const ImVec2 itemMax(itemMinX + itemW, itemMinY + itemH);
    const ImVec2 rectMin(itemMinX + cropRect_.x * scaleX, itemMinY + cropRect_.y * scaleY);
    const ImVec2 rectMax(itemMinX + cropRect_.Right() * scaleX, itemMinY + cropRect_.Bottom() * scaleY);

    // Dim everything outside the crop rect.
    const ImU32 dimColor = IM_COL32(0, 0, 0, 140);
    drawList->AddRectFilled(itemMin, ImVec2(itemMax.x, rectMin.y), dimColor);                  // top
    drawList->AddRectFilled(ImVec2(itemMin.x, rectMax.y), itemMax, dimColor);                  // bottom
    drawList->AddRectFilled(ImVec2(itemMin.x, rectMin.y), ImVec2(rectMin.x, rectMax.y), dimColor); // left
    drawList->AddRectFilled(ImVec2(rectMax.x, rectMin.y), ImVec2(itemMax.x, rectMax.y), dimColor); // right

    drawList->AddRect(rectMin, rectMax, IM_COL32(255, 255, 255, 230), 0.0f, 0, 1.5f);

    const float midX = (rectMin.x + rectMax.x) * 0.5f;
    const float midY = (rectMin.y + rectMax.y) * 0.5f;
    struct HandleDef {
        CropHandle id;
        ImVec2 pos;
    };
    const HandleDef handles[] = {
        {CropHandle::NW, {rectMin.x, rectMin.y}}, {CropHandle::N, {midX, rectMin.y}}, {CropHandle::NE, {rectMax.x, rectMin.y}},
        {CropHandle::W, {rectMin.x, midY}},                                           {CropHandle::E, {rectMax.x, midY}},
        {CropHandle::SW, {rectMin.x, rectMax.y}}, {CropHandle::S, {midX, rectMax.y}}, {CropHandle::SE, {rectMax.x, rectMax.y}},
    };

    constexpr float kHandleHalf = 5.0f;
    constexpr float kHandleHitHalf = 9.0f; // generous hit radius, easier to grab than the visible square
    for (const auto& h : handles) {
        const ImVec2 mn(h.pos.x - kHandleHalf, h.pos.y - kHandleHalf);
        const ImVec2 mx(h.pos.x + kHandleHalf, h.pos.y + kHandleHalf);
        drawList->AddRectFilled(mn, mx, IM_COL32(255, 255, 255, 255));
        drawList->AddRect(mn, mx, IM_COL32(20, 20, 20, 255));
    }

    const ImVec2 mouse = ImGui::GetMousePos();

    if (activeCropHandle_ == CropHandle::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mouse.x >= itemMin.x &&
        mouse.x <= itemMax.x && mouse.y >= itemMin.y && mouse.y <= itemMax.y) {
        CropHandle hit = CropHandle::None;
        for (const auto& h : handles) {
            if (std::fabs(mouse.x - h.pos.x) <= kHandleHitHalf && std::fabs(mouse.y - h.pos.y) <= kHandleHitHalf) {
                hit = h.id;
                break;
            }
        }
        if (hit == CropHandle::None && mouse.x >= rectMin.x && mouse.x <= rectMax.x && mouse.y >= rectMin.y &&
            mouse.y <= rectMax.y) {
            hit = CropHandle::Move;
        }
        if (hit != CropHandle::None) {
            activeCropHandle_ = hit;
            cropDragStartRect_ = cropRect_;
            cropDragStartMouseX_ = mouse.x;
            cropDragStartMouseY_ = mouse.y;
        }
    }

    if (activeCropHandle_ != CropHandle::None) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const int dx = static_cast<int>(std::lround((mouse.x - cropDragStartMouseX_) / scaleX));
            const int dy = static_cast<int>(std::lround((mouse.y - cropDragStartMouseY_) / scaleY));

            int x0 = cropDragStartRect_.x;
            int y0 = cropDragStartRect_.y;
            int x1 = cropDragStartRect_.Right();
            int y1 = cropDragStartRect_.Bottom();

            if (activeCropHandle_ == CropHandle::Move) {
                const int clampedDx = std::clamp(dx, -x0, imgW - x1);
                const int clampedDy = std::clamp(dy, -y0, imgH - y1);
                x0 += clampedDx;
                x1 += clampedDx;
                y0 += clampedDy;
                y1 += clampedDy;
            } else {
                constexpr int kMinSize = 4;
                const bool west = activeCropHandle_ == CropHandle::W || activeCropHandle_ == CropHandle::NW ||
                                   activeCropHandle_ == CropHandle::SW;
                const bool east = activeCropHandle_ == CropHandle::E || activeCropHandle_ == CropHandle::NE ||
                                   activeCropHandle_ == CropHandle::SE;
                const bool north = activeCropHandle_ == CropHandle::N || activeCropHandle_ == CropHandle::NW ||
                                    activeCropHandle_ == CropHandle::NE;
                const bool south = activeCropHandle_ == CropHandle::S || activeCropHandle_ == CropHandle::SW ||
                                    activeCropHandle_ == CropHandle::SE;
                if (west) x0 = std::clamp(x0 + dx, 0, x1 - kMinSize);
                if (east) x1 = std::clamp(x1 + dx, x0 + kMinSize, imgW);
                if (north) y0 = std::clamp(y0 + dy, 0, y1 - kMinSize);
                if (south) y1 = std::clamp(y1 + dy, y0 + kMinSize, imgH);
            }

            cropRect_ = CropRect{x0, y0, x1 - x0, y1 - y0};
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            activeCropHandle_ = CropHandle::None;
        }
    }
}

void Application::RebuildResultTexture() {
    if (resultTexture_) {
        SDL_DestroyTexture(resultTexture_);
        resultTexture_ = nullptr;
    }
    resultTexture_ = CreateTextureFromImage(renderer_, resultImage_);
}

void Application::DoOpenImage() {
    NFD::UniquePathN outPath;
    const nfdnfilteritem_t filters[] = {
        {PIXIE_NSTR("Image Files"), PIXIE_NSTR("png,jpg,jpeg,bmp,tga,gif,psd,hdr,pic,pgm,ppm")},
    };

    const nfdresult_t result = NFD::OpenDialog(outPath, filters, 1, kDefaultSourceDir);
    if (result == NFD_OKAY) {
        const std::filesystem::path path(outPath.get());
        std::string error;
        if (sourceImage_.LoadFromFile(path, &error)) {
            sourcePath_ = path;
            resultImage_.Clear();
            resultWidth_ = 0;
            resultHeight_ = 0;
            if (resultTexture_) {
                SDL_DestroyTexture(resultTexture_);
                resultTexture_ = nullptr;
            }
            backgroundMask_.Clear();
            if (removeBackgroundEnabled_) {
                RecomputeBackgroundMask();
            }
            cropRect_ = CropRect{};
            activeCropHandle_ = CropHandle::None;
            if (cropEnabled_) {
                cropRect_ = ComputeAutoCropRect(sourceImage_, static_cast<double>(cropTolerance_));
            }
            RebuildSourceTexture();
            statusMessage_ = "Loaded image: " + path.filename().string() + " (" +
                              std::to_string(sourceImage_.Width()) + "x" + std::to_string(sourceImage_.Height()) + ")";
        } else {
            statusMessage_ = "Failed to load image: " + error;
        }
    } else if (result == NFD_ERROR) {
        statusMessage_ = std::string("File dialog error: ") + NFD::GetError();
    }
}

void Application::DoOpenPalette() {
    NFD::UniquePathN outPath;
    const nfdnfilteritem_t filters[] = {
        {PIXIE_NSTR("GIMP Palette"), PIXIE_NSTR("gpl")},
    };

    const nfdresult_t result = NFD::OpenDialog(outPath, filters, 1, kDefaultPaletteDir);
    if (result == NFD_OKAY) {
        const std::filesystem::path path(outPath.get());
        std::string error;
        Palette loaded;
        if (loaded.LoadFromGpl(path, &error)) {
            palette_ = std::move(loaded);
            statusMessage_ = "Loaded palette: " + palette_.Name() + " (" + std::to_string(palette_.Colors().size()) + " colors)";
        } else {
            statusMessage_ = "Failed to load palette: " + error;
        }
    } else if (result == NFD_ERROR) {
        statusMessage_ = std::string("File dialog error: ") + NFD::GetError();
    }
}

void Application::DoSaveResult() {
    if (resultImage_.Empty()) {
        return;
    }

    NFD::UniquePathN outPath;
    const nfdnfilteritem_t filters[] = {
        {PIXIE_NSTR("PNG Image"), PIXIE_NSTR("png")},
    };

    const NativeString defaultName = sourcePath_.empty()
                                          ? NativeString(PIXIE_NSTR("converted.png"))
                                          : sourcePath_.stem().native() + NativeString(PIXIE_NSTR("_pixie.png"));

    const nfdresult_t result = NFD::SaveDialog(outPath, filters, 1, kDefaultConvertedDir, defaultName.c_str());
    if (result == NFD_OKAY) {
        const std::filesystem::path path(outPath.get());
        std::string error;
        if (resultImage_.SaveToPng(path, &error)) {
            statusMessage_ = "Saved: " + path.filename().string();
        } else {
            statusMessage_ = "Failed to save: " + error;
        }
    } else if (result == NFD_ERROR) {
        statusMessage_ = std::string("File dialog error: ") + NFD::GetError();
    }
}

void Application::DoConvert() {
    if (sourceImage_.Empty() || converting_) {
        return;
    }
    converting_ = true;
    statusMessage_ = "Converting...";

    // Snapshot a working copy up front (UI thread only): sourceImage_/
    // backgroundMask_/cropRect_ resume being mutable as soon as this call returns.
    const bool useCrop = cropEnabled_ && !cropRect_.Empty() &&
                          (cropRect_.x != 0 || cropRect_.y != 0 || cropRect_.width != sourceImage_.Width() ||
                           cropRect_.height != sourceImage_.Height());
    auto working = std::make_shared<Image>();
    if (useCrop) {
        CropImage(sourceImage_, cropRect_, *working);
    } else {
        *working = sourceImage_;
    }

    if (removeBackgroundEnabled_ && backgroundMask_.MatchesSize(sourceImage_.Width(), sourceImage_.Height())) {
        // Bake the mask's transparency into the working copy now, so both
        // downscale and quantize just see correct alpha, in either order.
        if (useCrop) {
            Mask croppedMask;
            CropMask(backgroundMask_, cropRect_, croppedMask);
            ApplyBackgroundMask(croppedMask, *working);
        } else {
            ApplyBackgroundMask(backgroundMask_, *working);
        }
    }

    const bool useDownscale = downscaleEnabled_;
    const int downscaleWidth = downscaleWidth_;
    const bool quantizeFirst = quantizeBeforeDownscale_;

    conversionFuture_ = std::async(std::launch::async, [this, working, useDownscale, downscaleWidth, quantizeFirst]() {
        Image stage = std::move(*working);

        auto quantize = [this](Image& img) {
            Image quantized;
            ConvertImage(img, palette_, quantized);
            img = std::move(quantized);
        };
        auto downscale = [](Image& img, int targetWidth) {
            const int h = ComputeMatchingHeight(img.Width(), img.Height(), targetWidth);
            Image resized;
            DownscaleImage(img, targetWidth, h, resized);
            img = std::move(resized);
        };

        if (quantizeFirst) {
            quantize(stage);
            if (useDownscale) downscale(stage, downscaleWidth);
        } else {
            if (useDownscale) downscale(stage, downscaleWidth);
            quantize(stage);
        }

        resultImage_ = std::move(stage);
    });
}

void Application::RenderUi() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    constexpr ImGuiWindowFlags kMainFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("PixieMain", nullptr, kMainFlags);

    ImGui::BeginDisabled(converting_);
    if (ImGui::Button("Open Image...")) {
        DoOpenImage();
    }
    ImGui::SameLine();
    if (ImGui::Button("Open Palette (.gpl)...")) {
        DoOpenPalette();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(sourceImage_.Empty() || converting_);
    if (ImGui::Button("Convert")) {
        DoConvert();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(converting_ || resultWidth_ <= 0);
    if (ImGui::Button("Save Result As PNG...")) {
        DoSaveResult();
    }
    ImGui::EndDisabled();

    if (converting_) {
        ImGui::SameLine();
        ImGui::TextUnformatted("Converting...");
    }

    if (!statusMessage_.empty()) {
        ImGui::TextWrapped("%s", statusMessage_.c_str());
    }

    ImGui::Separator();

    constexpr float kPaletteColumnWidth = 240.0f;
    ImGui::BeginChild("PaletteList", ImVec2(kPaletteColumnWidth, 0.0f), ImGuiChildFlags_Borders);
    ImGui::Text("Palette: %s", palette_.Empty() ? "(none)" : palette_.Name().c_str());
    ImGui::Text("%zu colors", palette_.Colors().size());
    ImGui::Separator();

    // Fixed to kPaletteRows rows; wider palettes pan via the horizontal
    // scrollbar instead of growing the column tall.
    constexpr int kPaletteRows = 5;
    constexpr float kSwatchSize = 20.0f;
    constexpr float kSwatchStride = kSwatchSize + 4.0f;
    const float paletteGridHeight = kPaletteRows * kSwatchStride + ImGui::GetStyle().ScrollbarSize;
    ImGui::BeginChild("PaletteSwatches", ImVec2(0.0f, paletteGridHeight), ImGuiChildFlags_None,
                       ImGuiWindowFlags_HorizontalScrollbar);
    const ImVec2 gridOrigin = ImGui::GetCursorPos();
    int swatchRow = 0;
    int swatchCol = 0;
    for (const PaletteColor& pc : palette_.Colors()) {
        ImGui::SetCursorPos(ImVec2(gridOrigin.x + swatchCol * kSwatchStride, gridOrigin.y + swatchRow * kSwatchStride));
        const ImVec4 color(pc.rgb.r / 255.0f, pc.rgb.g / 255.0f, pc.rgb.b / 255.0f, 1.0f);
        ImGui::PushID(&pc);
        ImGui::ColorButton("##swatch", color, ImGuiColorEditFlags_NoTooltip, ImVec2(kSwatchSize, kSwatchSize));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s\nRGB(%d, %d, %d)", pc.name.empty() ? "(unnamed)" : pc.name.c_str(), pc.rgb.r, pc.rgb.g,
                               pc.rgb.b);
        }
        ImGui::PopID();
        ++swatchRow;
        if (swatchRow >= kPaletteRows) {
            swatchRow = 0;
            ++swatchCol;
        }
    }
    ImGui::EndChild(); // PaletteSwatches

    ImGui::Separator();
    ImGui::TextUnformatted("Tools");
    ImGui::Spacing();

    ImGui::BeginDisabled(converting_ || sourceImage_.Empty());
    if (ImGui::Checkbox("Remove Background", &removeBackgroundEnabled_)) {
        if (removeBackgroundEnabled_ && !backgroundMask_.MatchesSize(sourceImage_.Width(), sourceImage_.Height())) {
            RecomputeBackgroundMask();
        }
        RebuildSourceTexture();
    }
    if (removeBackgroundEnabled_) {
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderFloat("##BackgroundTolerance", &backgroundTolerance_, 1.0f, 30.0f, "Tolerance %.1f");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            RecomputeBackgroundMask();
            RebuildSourceTexture();
        }
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BeginDisabled(converting_ || sourceImage_.Empty());
    if (ImGui::Checkbox("Crop", &cropEnabled_)) {
        if (cropEnabled_ && cropRect_.Empty()) {
            cropRect_ = ComputeAutoCropRect(sourceImage_, static_cast<double>(cropTolerance_));
        }
        activeCropHandle_ = CropHandle::None;
    }
    if (cropEnabled_) {
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderFloat("##CropTolerance", &cropTolerance_, 1.0f, 30.0f, "Tolerance %.1f");
        if (ImGui::Button("Auto Crop")) {
            cropRect_ = ComputeAutoCropRect(sourceImage_, static_cast<double>(cropTolerance_));
            activeCropHandle_ = CropHandle::None;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Crop")) {
            cropRect_ = CropRect{0, 0, sourceImage_.Width(), sourceImage_.Height()};
            activeCropHandle_ = CropHandle::None;
        }
        ImGui::Text("(%d, %d) %dx%d", cropRect_.x, cropRect_.y, cropRect_.width, cropRect_.height);
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BeginDisabled(converting_ || sourceImage_.Empty());
    ImGui::Checkbox("Downscale", &downscaleEnabled_);
    if (downscaleEnabled_) {
        struct DownscaleOption {
            int width;
            const char* label;
        };
        constexpr DownscaleOption kDownscaleOptions[] = {
            {16, "16"},
            {32, "32"},
            {64, "64"},
            {128, "128"},
        };
        constexpr int kDownscaleOptionCount = 4;

        for (int i = 0; i < kDownscaleOptionCount; ++i) {
            const DownscaleOption& option = kDownscaleOptions[i];
            if (ImGui::RadioButton(option.label, downscaleWidth_ == option.width)) {
                downscaleWidth_ = option.width;
            }
            if (i + 1 < kDownscaleOptionCount) {
                ImGui::SameLine();
            }
        }
        const int previewHeight = ComputeMatchingHeight(sourceImage_.Width(), sourceImage_.Height(), downscaleWidth_);
        ImGui::Text("-> %dx%d", downscaleWidth_, previewHeight);

        ImGui::BeginDisabled(palette_.Empty());
        ImGui::Checkbox("Swap palette before downscale", &quantizeBeforeDownscale_);
        ImGui::EndDisabled();
    }
    ImGui::EndDisabled();

    ImGui::EndChild(); // PaletteList

    ImGui::SameLine();

    ImGui::BeginChild("Previews", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    const float halfWidth = ImGui::GetContentRegionAvail().x * 0.5f - 4.0f;

    ImGui::BeginChild("SourcePreview", ImVec2(halfWidth, 0.0f), ImGuiChildFlags_Borders);
    ImGui::Text("Source (%dx%d)", sourceImage_.Width(), sourceImage_.Height());
    if (cropEnabled_) {
        ImGui::TextDisabled("Drag the handles to adjust the crop");
    } else if (removeBackgroundEnabled_) {
        ImGui::TextDisabled("Click the image to include/exclude a region");
    }
    if (sourceTexture_) {
        const ImVec2 imgSize = FitToRegion(sourceImage_.Width(), sourceImage_.Height(), ImGui::GetContentRegionAvail());
        ImGui::Image((ImTextureID)(intptr_t)sourceTexture_, imgSize);

        const ImVec2 itemMin = ImGui::GetItemRectMin();
        const ImVec2 itemSize = ImGui::GetItemRectSize();

        if (cropEnabled_ && !converting_ && itemSize.x > 0.0f && itemSize.y > 0.0f) {
            RenderCropOverlay(ImGui::GetWindowDrawList(), itemMin.x, itemMin.y, itemSize.x, itemSize.y);
        } else if (removeBackgroundEnabled_ && !converting_ && !backgroundMask_.Empty() &&
                   ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            if (itemSize.x > 0.0f && itemSize.y > 0.0f) {
                const ImVec2 mousePos = ImGui::GetMousePos();
                const float relX = (mousePos.x - itemMin.x) / itemSize.x;
                const float relY = (mousePos.y - itemMin.y) / itemSize.y;
                const int px = std::clamp(static_cast<int>(relX * sourceImage_.Width()), 0, sourceImage_.Width() - 1);
                const int py = std::clamp(static_cast<int>(relY * sourceImage_.Height()), 0, sourceImage_.Height() - 1);
                ToggleRegion(sourceImage_, px, py, static_cast<double>(backgroundTolerance_), backgroundMask_);
                RebuildSourceTexture();
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("ResultPreview", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    ImGui::Text("Converted (%dx%d)", resultWidth_, resultHeight_);
    if (resultTexture_) {
        const ImVec2 imgSize = FitToRegion(resultImage_.Width(), resultImage_.Height(), ImGui::GetContentRegionAvail());
        ImGui::Image((ImTextureID)(intptr_t)resultTexture_, imgSize);
    }
    ImGui::EndChild();

    ImGui::EndChild(); // Previews

    ImGui::End();
}

} // namespace pixie
