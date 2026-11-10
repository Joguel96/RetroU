#include "gx2/gx2_renderer.hpp"
#include "gx2/gx2_text.hpp"
#include "gx2/gx2_image.hpp"
#include "gx2/retrou_data.hpp"
#include "rpl_core.hpp"

#include <SDL.h>
#include <libretro.h>
#include <vpad/input.h>

#include <string>

// First GX2 core integration test (NES): renders the library, and on A loads
// the first ROM and presents the core frame through the GX2 renderer (single
// TV buffer copied to the GamePad).

extern const unsigned char _binary_ui_ttf_start[];
extern const unsigned char _binary_ui_ttf_end[];

using namespace retrou;
using namespace retrou::gx2;

static SDL_GameController *openGameController()
{
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            SDL_GameController *c = SDL_GameControllerOpen(i);
            if (c) return c;
        }
    }
    return nullptr;
}

int main()
{
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS);
    SDL_GameControllerEventState(SDL_ENABLE);

    if (!init()) return 1;
    if (!text::loadFont(_binary_ui_ttf_start,
                        static_cast<size_t>(_binary_ui_ttf_end - _binary_ui_ttf_start))) {
        return 1;
    }

    Library library;
    library.sd_root = mountSd();
    scanRoms(library, "nes");

    RplCore core;
    std::string error;
    if (!core.loadStaticCore(nullptr, nullptr, error)) {
        return 1;
    }
    core.setSaveDirectory("/vol/external01/retrou/saves/nes/save");

    SDL_GameController *controller = openGameController();
    core.setControllers(controller, nullptr);

    bool gameRunning = false;
    Texture frameTexture = -1;

    if (!library.roms.empty()) {
        if (core.loadGame(library.roms[0].path, error)) {
            gameRunning = true;
        }
    }

    while (true) {
        VPADStatus vpad{};
        VPADReadError vpadError = VPAD_READ_NO_SAMPLES;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &vpadError);
        if (vpadError == VPAD_READ_SUCCESS &&
            (vpad.hold & VPAD_BUTTON_MINUS) && (vpad.hold & VPAD_BUTTON_PLUS)) {
            break;
        }

        if (gameRunning) {
            core.runFrame();
        }

        beginFrame(Color{0, 0, 0, 255});
        if (gameRunning && core.hasFrame()) {
            const bool rgb565 = core.pixelFormat() == RETRO_PIXEL_FORMAT_RGB565;
            frameTexture = uploadFrame(frameTexture,
                                       core.frameData(),
                                       static_cast<int>(core.frameWidth()),
                                       static_cast<int>(core.frameHeight()),
                                       static_cast<int>(core.framePitch()),
                                       rgb565);
            if (frameTexture >= 0) {
                drawTexture(frameTexture, 0.0f, 0.0f, 1280.0f, 720.0f);
            }
        } else {
            if (library.roms.empty()) {
                text::drawText("Sin ROMs en sd:/retrou/roms/nes/",
                               100.0f, 340.0f, 24, Color{255, 255, 255, 255});
            } else {
                text::drawText("Error al cargar el ROM", 100.0f, 340.0f, 24,
                               Color{255, 255, 255, 255});
            }
            text::drawText("MINUS + PLUS para salir", 100.0f, 380.0f, 18,
                           Color{148, 163, 184, 255});
        }
        endFrame();
    }

    core.unload();
    if (controller) SDL_GameControllerClose(controller);
    SDL_Quit();
    return 0;
}
