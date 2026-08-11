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

namespace pixie {

namespace {

#ifdef _WIN32
#define PIXIE_NSTR(literal) L##literal
#else
#define PIXIE_NSTR(literal) literal
#endif

using NativeString = std::filesystem::path::string_type;

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
    statusMessage_ = "Converted to " + std::to_string(resultWidth_) + "x" + std::to_string(resultHeight_) +
                      " using palette \"" + palette_.Name() + "\"";
}

void Application::RebuildSourceTexture() {
    if (sourceTexture_) {
        SDL_DestroyTexture(sourceTexture_);
        sourceTexture_ = nullptr;
    }
    sourceTexture_ = CreateTextureFromImage(renderer_, sourceImage_);
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

    const nfdresult_t result = NFD::OpenDialog(outPath, filters, 1, nullptr);
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

    const nfdresult_t result = NFD::OpenDialog(outPath, filters, 1, nullptr);
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

    const nfdresult_t result = NFD::SaveDialog(outPath, filters, 1, nullptr, defaultName.c_str());
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
    if (sourceImage_.Empty() || palette_.Empty() || converting_) {
        return;
    }
    converting_ = true;
    statusMessage_ = "Converting...";
    conversionFuture_ = std::async(std::launch::async, [this]() {
        ConvertImage(sourceImage_, palette_, resultImage_);
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
    ImGui::BeginDisabled(sourceImage_.Empty() || palette_.Empty() || converting_);
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

    constexpr float kSwatchSize = 20.0f;
    const float availWidth = ImGui::GetContentRegionAvail().x;
    const int perRow = std::max(1, static_cast<int>(availWidth / (kSwatchSize + 4.0f)));
    int column = 0;
    for (const PaletteColor& pc : palette_.Colors()) {
        const ImVec4 color(pc.rgb.r / 255.0f, pc.rgb.g / 255.0f, pc.rgb.b / 255.0f, 1.0f);
        ImGui::PushID(&pc);
        ImGui::ColorButton("##swatch", color, ImGuiColorEditFlags_NoTooltip, ImVec2(kSwatchSize, kSwatchSize));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s\nRGB(%d, %d, %d)", pc.name.empty() ? "(unnamed)" : pc.name.c_str(), pc.rgb.r, pc.rgb.g,
                               pc.rgb.b);
        }
        ImGui::PopID();
        ++column;
        if (column < perRow) {
            ImGui::SameLine();
        } else {
            column = 0;
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("Previews", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    const float halfWidth = ImGui::GetContentRegionAvail().x * 0.5f - 4.0f;

    ImGui::BeginChild("SourcePreview", ImVec2(halfWidth, 0.0f), ImGuiChildFlags_Borders);
    ImGui::Text("Source (%dx%d)", sourceImage_.Width(), sourceImage_.Height());
    if (sourceTexture_) {
        const ImVec2 imgSize = FitToRegion(sourceImage_.Width(), sourceImage_.Height(), ImGui::GetContentRegionAvail());
        ImGui::Image((ImTextureID)(intptr_t)sourceTexture_, imgSize);
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
