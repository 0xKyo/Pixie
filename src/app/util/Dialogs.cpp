#include "app/util/Dialogs.h"

#include <string>

namespace pixie::util {

bool ConfirmOverwrite(SDL_Window* window, int count) {
    const std::string message =
        count == 1 ? "1 .aseprite file already exists next to the original and will be replaced. Any edits made to it in Aseprite will be lost."
                   : std::to_string(count) + " .aseprite files already exist next to the originals and will be replaced. Any edits made to them in Aseprite will be lost.";
    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"},
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Overwrite"},
    };
    SDL_MessageBoxData box{};
    box.flags = SDL_MESSAGEBOX_WARNING;
    box.window = window;
    box.title = "Pixie";
    box.message = message.c_str();
    box.numbuttons = 2;
    box.buttons = buttons;

    int chosenButton = 0;
    return SDL_ShowMessageBox(&box, &chosenButton) && chosenButton == 1;
}

} // namespace pixie::util
