#include "gx2/gx2_renderer.hpp"
#include "gx2/gx2_text.hpp"
#include "gx2/gx2_image.hpp"
#include "gx2/retrou_data.hpp"
#include "rpl_core.hpp"

#include <SDL.h>
#include <libretro.h>
#include <vpad/input.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

// GX2 NES frontend: library grid with covers and focus, A launches the
// focused game, B returns to the library, Minus+Plus exits.

extern const unsigned char _binary_ui_ttf_start[];
extern const unsigned char _binary_ui_ttf_end[];

using namespace retrou;
using namespace retrou::gx2;

#ifndef RETROU_CORE_SYSTEM
#define RETROU_CORE_SYSTEM "nes"
#endif
#ifndef RETROU_CORE_LABEL
#define RETROU_CORE_LABEL "NES"
#endif

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

static Texture makeCover(uint8_t r0, uint8_t g0, uint8_t b0,
                         uint8_t r1, uint8_t g1, uint8_t b1)
{
    constexpr int w = 96, h = 128;
    std::vector<uint8_t> px(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) {
        const float t = static_cast<float>(y) / (h - 1);
        for (int x = 0; x < w; ++x) {
            uint8_t *p = &px[(static_cast<size_t>(y) * w + x) * 4];
            p[0] = static_cast<uint8_t>(r0 * (1.0f - t) + r1 * t);
            p[1] = static_cast<uint8_t>(g0 * (1.0f - t) + g1 * t);
            p[2] = static_cast<uint8_t>(b0 * (1.0f - t) + b1 * t);
            p[3] = 255;
        }
    }
    return createTexture(w, h, px.data());
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
    scanRoms(library, RETROU_CORE_SYSTEM);

    const int maxCards = 10;
    const int count = std::min(maxCards, static_cast<int>(library.roms.size()));

    // Layout.
    const int columns = 5, rows = 2;
    const float gridX = 344.0f, gridY = 88.0f;
    const float gridW = 1280.0f - gridX - 28.0f, gridH = 560.0f;
    const float gap = 14.0f;
    const float cardW = (gridW - gap * (columns - 1)) / columns;
    const float cardH = (gridH - gap * (rows - 1)) / rows;
    const float coverH = cardH * 0.62f;

    // Covers: real PNG when present, otherwise a procedural gradient.
    std::vector<Texture> covers(count);
    for (int i = 0; i < count; ++i) {
        std::string cp = coverPath(library.sd_root, RETROU_CORE_SYSTEM, library.roms[i].title);
        Texture real = image::loadFromFile(cp.c_str());
        if (real >= 0) {
            covers[i] = real;
        } else {
            const uint8_t r = static_cast<uint8_t>(30 + (i * 37) % 160);
            const uint8_t g = static_cast<uint8_t>(60 + (i * 29) % 140);
            const uint8_t b = static_cast<uint8_t>(120 - (i * 23) % 100);
            covers[i] = makeCover(r, g, b, 10, 14, 24);
        }
    }

    std::vector<std::string> titles(count);
    for (int i = 0; i < count; ++i) {
        titles[i] = text::fitText(library.roms[i].title.c_str(), 15, cardW - 24.0f);
        text::prewarm(titles[i].c_str(), 15);
        text::prewarm(RETROU_CORE_LABEL, 12);
    }
    text::prewarm("Biblioteca", 34);
    text::prewarm("FILTRAR Y ORGANIZAR", 16);
    text::prewarm("Sin ROMs en sd:/retrou/roms/" RETROU_CORE_SYSTEM "/", 24);
    text::prewarm("A  JUGAR    B  VOLVER    MINUS+PLUS  SALIR", 15);

    RplCore core;
    std::string error;
    if (!core.loadStaticCore(nullptr, nullptr, error)) {
        return 1;
    }
    core.setSaveDirectory("/vol/external01/retrou/saves/" RETROU_CORE_SYSTEM "/save");
    SDL_GameController *controller = openGameController();
    core.setControllers(controller, nullptr);

    int focused = 0;
    bool inGame = false;
    Texture frameTex = -1;

    const Color bg{8, 11, 18, 255};
    const Color accent{245, 217, 127, 255};
    const Color white{255, 255, 255, 255};
    const Color muted{148, 163, 184, 255};
    const Color panel{21, 27, 40, 235};
    const Color cardPanel{16, 21, 33, 235};
    const Color cardFooter{10, 14, 24, 235};

    while (true) {
        VPADStatus vpad{};
        VPADReadError vpadError = VPAD_READ_NO_SAMPLES;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &vpadError);
        if (vpadError == VPAD_READ_SUCCESS) {
            if ((vpad.hold & VPAD_BUTTON_MINUS) && (vpad.hold & VPAD_BUTTON_PLUS)) {
                break;
            }
            if (inGame) {
                if (vpad.trigger & VPAD_BUTTON_B) {
                    core.unload();
                    core.loadStaticCore(nullptr, nullptr, error);
                    core.setSaveDirectory("/vol/external01/retrou/saves/" RETROU_CORE_SYSTEM "/save");
                    core.setControllers(controller, nullptr);
                    inGame = false;
                }
            } else {
                if (vpad.trigger & VPAD_BUTTON_A && count > 0) {
                    if (core.loadGame(library.roms[focused].path, error)) {
                        inGame = true;
                    }
                }
                if (count > 0) {
                    if (vpad.trigger & VPAD_BUTTON_RIGHT) focused = (focused + 1) % count;
                    if (vpad.trigger & VPAD_BUTTON_LEFT) focused = (focused + count - 1) % count;
                    if (vpad.trigger & VPAD_BUTTON_DOWN) focused = std::min(count - 1, focused + columns);
                    if (vpad.trigger & VPAD_BUTTON_UP) focused = std::max(0, focused - columns);
                }
            }
        }

        if (inGame) {
            core.runFrame();
        }

        beginFrame(bg);

        if (inGame && core.hasFrame()) {
            const bool rgb565 = core.pixelFormat() == RETRO_PIXEL_FORMAT_RGB565;
            frameTex = uploadFrame(frameTex, core.frameData(),
                                   static_cast<int>(core.frameWidth()),
                                   static_cast<int>(core.frameHeight()),
                                   static_cast<int>(core.framePitch()), rgb565);
            if (frameTex >= 0) {
                drawTexture(frameTex, 0.0f, 0.0f, 1280.0f, 720.0f);
            }
        } else {
            text::drawText("Biblioteca", 28.0f, 18.0f, 34, white);
            drawRoundRect(24.0f, 88.0f, 300.0f, 560.0f, 18.0f, panel);
            text::drawText("FILTRAR Y ORGANIZAR", 44.0f, 106.0f, 16, muted);

            if (count == 0) {
                text::drawText("Sin ROMs en sd:/retrou/roms/" RETROU_CORE_SYSTEM "/", 100.0f, 340.0f, 24, white);
            } else {
                for (int i = 0; i < count; ++i) {
                    const int col = i % columns, row = i / columns;
                    const float x = gridX + col * (cardW + gap);
                    const float y = gridY + row * (cardH + gap);
                    const bool sel = i == focused;
                    drawRoundRect(x - 3.0f, y - 3.0f, cardW + 6.0f, cardH + 6.0f, 15.0f,
                                  sel ? accent : Color{255, 255, 255, 44});
                    drawRoundRect(x, y, cardW, cardH, 12.0f, cardPanel);
                    drawTextureCover(covers[i], x + 2.0f, y + 2.0f, cardW - 4.0f, coverH - 2.0f);
                    drawRoundRect(x + 2.0f, y + coverH, cardW - 4.0f, cardH - coverH - 2.0f, 10.0f, cardFooter);
                    text::drawText(titles[i].c_str(), x + 12.0f, y + coverH + 12.0f, 15, white);
                    text::drawText(RETROU_CORE_LABEL, x + 12.0f, y + coverH + 34.0f, 12, muted);
                }
            }
            text::drawText("A  JUGAR    B  VOLVER    MINUS+PLUS  SALIR",
                           28.0f, 672.0f, 15, muted);
        }

        endFrame();
    }

    core.unload();
    if (controller) SDL_GameControllerClose(controller);
    SDL_Quit();
    return 0;
}
