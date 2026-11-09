#pragma once

#include <string>

#include "app/AppContext.h"
#include "app/views/ConverterView.h"
#include "app/views/LibraryView.h"
#include "app/views/PreviewPanel.h"

namespace pixie::app {

// Owns the window, renderer, Dear ImGui and file dialogs, runs the main loop and shows the current screen.
class Application {
public:
    Application() = default;
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool Init(); // false on failure; see LastError()
    int Run();   // returns the process exit code when the window closes

    [[nodiscard]] const std::string& LastError() const { return lastError_; }

private:
    bool CreateWindowAndRenderer();
    void InitImGui();
    void Shutdown();
    void RenderCurrentScreen();

    AppContext context_;
    views::ConverterView converter_{context_};
    views::PreviewPanel preview_{context_, converter_};
    views::LibraryView library_{context_, converter_, preview_};

    bool imguiReady_ = false;
    bool fileDialogsReady_ = false;
    std::string lastError_;
};

} // namespace pixie::app
