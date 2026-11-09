#include "app/Application.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <nfd.hpp>

#include <filesystem>

#include "core/formats/ImageFile.h"

namespace pixie::app {

Application::~Application() {
    if (context_.window || context_.renderer) Shutdown();
}

bool Application::Init() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        lastError_ = SDL_GetError();
        return false;
    }
    SDL_InitSubSystem(SDL_INIT_AUDIO); // optional: without it audio just can't be played
    if (!CreateWindowAndRenderer()) return false;
    InitImGui();

    if (NFD::Init() != NFD_OKAY) {
        lastError_ = "Failed to initialize the native file dialog library";
        return false;
    }
    fileDialogsReady_ = true;

    converter_.LoadStartupPalette();
    context_.settings.Load();
    library_.LoadSettings();
    if (!context_.OpenSavedLibrary()) context_.screen = Screen::FolderPicker; // first run, or the folder is gone
    return true;
}

bool Application::CreateWindowAndRenderer() {
    const SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    context_.window = SDL_CreateWindow("Pixie - Pixel Art Asset Viewer", 1280, 800, flags);
    if (!context_.window) {
        lastError_ = SDL_GetError();
        return false;
    }
    context_.renderer = SDL_CreateRenderer(context_.window, nullptr);
    if (!context_.renderer) {
        lastError_ = SDL_GetError();
        return false;
    }
    SDL_SetRenderVSync(context_.renderer, 1);
    SDL_SetWindowPosition(context_.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_MaximizeWindow(context_.window);
    SetWindowIcon();
    SDL_ShowWindow(context_.window); // created hidden so it never flashes at the small size
    return true;
}

// Loads icon/pixie.png from next to the executable; a missing file just leaves the default icon.
void Application::SetWindowIcon() {
    const char* executableFolder = SDL_GetBasePath(); // owned by SDL
    if (!executableFolder) return;
    graphics::Image icon;
    if (!formats::LoadImageFile(std::filesystem::path(executableFolder) / "icon" / "pixie.png", icon)) return;

    SDL_Surface* surface = SDL_CreateSurfaceFrom(icon.Width(), icon.Height(), SDL_PIXELFORMAT_RGBA32,
                                                 icon.MutablePixels(), icon.Width() * 4);
    if (!surface) return;
    SDL_SetWindowIcon(context_.window, surface); // SDL copies the pixels, so `icon` can go out of scope
    SDL_DestroySurface(surface);
}

void Application::InitImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(context_.window, context_.renderer);
    ImGui_ImplSDLRenderer3_Init(context_.renderer);
    imguiReady_ = true;
}

// Order matters: everything holding textures goes before the renderer.
void Application::Shutdown() {
    converter_.Shutdown();
    library_.SaveSettings();
    preview_.Close();
    context_.thumbnails.Clear();

    if (fileDialogsReady_) {
        NFD::Quit();
        fileDialogsReady_ = false;
    }
    if (imguiReady_) {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
    if (context_.renderer) {
        SDL_DestroyRenderer(context_.renderer);
        context_.renderer = nullptr;
    }
    if (context_.window) {
        SDL_DestroyWindow(context_.window);
        context_.window = nullptr;
    }
    SDL_Quit();
}

int Application::Run() {
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            const bool windowClosed = event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                                      event.window.windowID == SDL_GetWindowID(context_.window);
            if (event.type == SDL_EVENT_QUIT || windowClosed) running = false;
        }

        if (SDL_GetWindowFlags(context_.window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10); // nothing to draw; don't spin the CPU
            continue;
        }

        converter_.Update();

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        RenderCurrentScreen();
        ImGui::Render();

        SDL_SetRenderDrawColor(context_.renderer, 30, 30, 34, 255);
        SDL_RenderClear(context_.renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), context_.renderer);
        SDL_RenderPresent(context_.renderer);
    }

    Shutdown();
    return 0;
}

void Application::RenderCurrentScreen() {
    switch (context_.screen) {
    case Screen::FolderPicker: library_.RenderFolderPicker(); break;
    case Screen::Library: library_.Render(); break;
    case Screen::Converter: converter_.Render(); break;
    }
}

} // namespace pixie::app
