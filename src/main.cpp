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
    pixie::app::Application application;
    if (!application.Init()) {
        const std::string message = "Failed to initialize Pixie: " + application.LastError();
        std::fprintf(stderr, "%s\n", message.c_str());
#ifdef _WIN32
        MessageBoxA(nullptr, message.c_str(), "Pixie", MB_OK | MB_ICONERROR); // there is no console to print to
#endif
        return 1;
    }
    return application.Run();
}
