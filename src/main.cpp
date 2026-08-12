#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "app/Application.h"

int main(int, char**) {
    pixie::Application app;
    if (!app.Init()) {
        const std::string message = "Failed to initialize Pixie: " + app.LastError();
        std::fprintf(stderr, "%s\n", message.c_str());
#ifdef _WIN32
        // Pixie runs without an attached console, so stderr alone would be invisible.
        MessageBoxA(nullptr, message.c_str(), "Pixie", MB_OK | MB_ICONERROR);
#endif
        return 1;
    }
    return app.Run();
}
