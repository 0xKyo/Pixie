#include <SDL3/SDL.h>

#include <cstdio>

#include "app/Application.h"

int main(int, char**) {
    pixie::Application app;
    if (!app.Init()) {
        std::fprintf(stderr, "Failed to initialize Pixie: %s\n", app.LastError().c_str());
        return 1;
    }
    return app.Run();
}
