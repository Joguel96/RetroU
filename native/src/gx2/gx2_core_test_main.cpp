#include "gx2/gx2_renderer.hpp"
#include "gx2/gx2_text.hpp"
#include "gx2/gx2_image.hpp"
#include "gx2/retrou_data.hpp"
#include "rpl_core.hpp"

#include <SDL.h>
#include <coreinit/systeminfo.h>
#include <libretro.h>
#include <vpad/input.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// GX2 frontend per console: library grid with covers/focus, favorites, and a
// sidebar. A launches, B returns, X toggles favorite, Plus opens the sidebar,
// Minus+Plus exits.

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

// Region id: 0 unknown, 1 USA, 2 Europe, 3 Japan.
static int detectRegion(const std::string &title)
{
    std::string t;
    t.reserve(title.size());
    for (char c : title) {
        t += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (t.find("(usa)") != std::string::npos || t.find("[usa]") != std::string::npos ||
        t.find("(u)") != std::string::npos) {
        return 1;
    }
    if (t.find("(europe)") != std::string::npos || t.find("(spain)") != std::string::npos ||
        t.find("(france)") != std::string::npos || t.find("(germany)") != std::string::npos ||
        t.find("(italy)") != std::string::npos || t.find("(australia)") != std::string::npos ||
        t.find("(world)") != std::string::npos || t.find("(scandinavia)") != std::string::npos ||
        t.find("(europe") != std::string::npos) {
        return 2;
    }
    if (t.find("(japan)") != std::string::npos || t.find("[japan]") != std::string::npos ||
        t.find("(j)") != std::string::npos) {
        return 3;
    }
    return 0;
}

int main()
{
    OSEnableHomeButtonMenu(TRUE);
    SDL_Init(SDL_INIT_AUDIO);

    if (!init()) return 1;
    if (!text::loadFont(_binary_ui_ttf_start,
                        static_cast<size_t>(_binary_ui_ttf_end - _binary_ui_ttf_start))) {
        return 1;
    }

    Library library;
    library.sd_root = mountSd();
    scanRoms(library, RETROU_CORE_SYSTEM);
    loadFavorites(library, RETROU_CORE_SYSTEM);

    std::vector<int> visible;
    for (int i = 0; i < static_cast<int>(library.roms.size()); ++i) {
        visible.push_back(i);
    }
    bool favoritesOnly = false;
    bool sidebarOpen = false;
    int sidebarIndex = 0;
    int focused = 0;
    int sortMode = 0;      // 0 name, 1 region
    int regionFilter = 0;  // 0 all, 1 USA, 2 Europe, 3 Japan

    auto rebuildVisible = [&]() {
        visible.clear();
        for (int i = 0; i < static_cast<int>(library.roms.size()); ++i) {
            if (favoritesOnly && !library.roms[i].favorite) continue;
            if (regionFilter != 0 && detectRegion(library.roms[i].title) != regionFilter) {
                continue;
            }
            visible.push_back(i);
        }
        if (sortMode == 0) {
            std::sort(visible.begin(), visible.end(), [&](int a, int b) {
                return library.roms[a].title < library.roms[b].title;
            });
        } else {
            std::sort(visible.begin(), visible.end(), [&](int a, int b) {
                const int ka = detectRegion(library.roms[a].title);
                const int kb = detectRegion(library.roms[b].title);
                const int oa = ka == 0 ? 4 : ka;
                const int ob = kb == 0 ? 4 : kb;
                if (oa != ob) return oa < ob;
                return library.roms[a].title < library.roms[b].title;
            });
        }
        if (focused >= static_cast<int>(visible.size())) focused = 0;
    };

    const int columns = 5, rows = 2;
    const float gridX = 24.0f, gridY = 88.0f;
    const float gridW = 1280.0f - 48.0f, gridH = 560.0f;
    const float gap = 14.0f;
    const float cardW = (gridW - gap * (columns - 1)) / columns;
    const float cardH = (gridH - gap * (rows - 1)) / rows;
    const float coverH = cardH * 0.62f;

    std::map<int, Texture> coverCache;
    auto getCover = [&](int romIndex) -> Texture {
        auto it = coverCache.find(romIndex);
        if (it != coverCache.end()) return it->second;
        std::string cp = coverPath(library.sd_root, RETROU_CORE_SYSTEM, library.roms[romIndex].title);
        Texture real = image::loadFromFile(cp.c_str());
        if (real < 0) {
            const uint8_t r = static_cast<uint8_t>(30 + (romIndex * 37) % 160);
            const uint8_t g = static_cast<uint8_t>(60 + (romIndex * 29) % 140);
            const uint8_t b = static_cast<uint8_t>(120 - (romIndex * 23) % 100);
            real = makeCover(r, g, b, 10, 14, 24);
        }
        coverCache[romIndex] = real;
        return real;
    };

    std::map<int, std::string> titleCache;
    auto getTitle = [&](int romIndex) -> const std::string & {
        auto it = titleCache.find(romIndex);
        if (it != titleCache.end()) return it->second;
        titleCache[romIndex] =
            text::fitText(library.roms[romIndex].title.c_str(), 15, cardW - 24.0f);
        return titleCache[romIndex];
    };

    text::prewarm("Biblioteca", 34);
    text::prewarm("FILTRAR Y ORGANIZAR", 16);
    text::prewarm("Buscar", 15);
    text::prewarm("Ordenar por", 15);
    text::prewarm("Región", 15);
    text::prewarm("Favoritos", 15);
    text::prewarm("Nombre", 15);
    text::prewarm("Todas", 15);
    text::prewarm("USA", 15);
    text::prewarm("Europa", 15);
    text::prewarm("Japón", 15);
    text::prewarm("Sí", 15);
    text::prewarm("No", 15);
    text::prewarm("Sin ROMs", 24);
    text::prewarm("A  JUGAR    B  VOLVER    X  FAVORITO    +  FILTROS", 15);
    text::prewarm("MENU", 26);
    text::prewarm("Continuar", 20);
    text::prewarm("Cerrar core", 20);
    text::prewarm(RETROU_CORE_LABEL, 12);

    RplCore core;
    std::string error;
    if (!core.loadStaticCore(nullptr, nullptr, error)) {
        return 1;
    }
    core.setSaveDirectory("/vol/external01/retrou/saves/" RETROU_CORE_SYSTEM "/save");
    core.setControllers(nullptr, nullptr);
    core.setVpadEnabled(true);

    bool inGame = false;
    bool coreMenu = false;
    int coreMenuIndex = 0;
    Texture frameTex = -1;

    const Color bg{8, 11, 18, 255};
    const Color accent{245, 217, 127, 255};
    const Color white{255, 255, 255, 255};
    const Color muted{148, 163, 184, 255};
    const Color panel{21, 27, 40, 235};
    const Color cardPanel{16, 21, 33, 235};
    const Color cardFooter{10, 14, 24, 235};
    const Color gold{245, 217, 127, 255};

    while (true) {
        VPADStatus vpad{};
        VPADReadError vpadError = VPAD_READ_NO_SAMPLES;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &vpadError);
        if (vpadError == VPAD_READ_SUCCESS) {
            if (inGame) {
                if (coreMenu) {
                    if (vpad.trigger & (VPAD_BUTTON_UP | VPAD_BUTTON_DOWN)) {
                        coreMenuIndex = 1 - coreMenuIndex;
                    } else if (vpad.trigger & VPAD_BUTTON_A) {
                        if (coreMenuIndex == 0) {
                            coreMenu = false;
                        } else {
                            core.unloadGame();
                            inGame = false;
                            coreMenu = false;
                        }
                    } else if (vpad.trigger & (VPAD_BUTTON_B | VPAD_BUTTON_MINUS | VPAD_BUTTON_PLUS)) {
                        coreMenu = false;
                    }
                } else {
                    if ((vpad.trigger & VPAD_BUTTON_MINUS) && (vpad.trigger & VPAD_BUTTON_PLUS)) {
                        coreMenu = true;
                        coreMenuIndex = 0;
                    }
                }
            } else if (sidebarOpen) {
                if (vpad.trigger & VPAD_BUTTON_B) {
                    sidebarOpen = false;
                } else if (vpad.trigger & VPAD_BUTTON_DOWN) {
                    sidebarIndex = (sidebarIndex + 1) % 4;
                } else if (vpad.trigger & VPAD_BUTTON_UP) {
                    sidebarIndex = (sidebarIndex + 3) % 4;
                } else if (vpad.trigger & VPAD_BUTTON_A) {
                    if (sidebarIndex == 1) {
                        sortMode = (sortMode + 1) % 2;
                        rebuildVisible();
                    } else if (sidebarIndex == 2) {
                        regionFilter = (regionFilter + 1) % 4;
                        rebuildVisible();
                    } else if (sidebarIndex == 3) {
                        favoritesOnly = !favoritesOnly;
                        rebuildVisible();
                    }
                }
            } else {
                const int rc = std::min(10, static_cast<int>(visible.size()));
                if (vpad.trigger & VPAD_BUTTON_PLUS) {
                    sidebarOpen = true;
                    sidebarIndex = 0;
                } else if (rc > 0) {
                    if (vpad.trigger & VPAD_BUTTON_A) {
                        if (core.loadGame(library.roms[visible[focused]].path, error)) {
                            inGame = true;
                        }
                    } else if (vpad.trigger & VPAD_BUTTON_X) {
                        RomEntry &rom = library.roms[visible[focused]];
                        rom.favorite = !rom.favorite;
                        saveFavorites(library, RETROU_CORE_SYSTEM);
                    } else if (vpad.trigger & VPAD_BUTTON_RIGHT) {
                        focused = (focused + 1) % rc;
                    } else if (vpad.trigger & VPAD_BUTTON_LEFT) {
                        focused = (focused + rc - 1) % rc;
                    } else if (vpad.trigger & VPAD_BUTTON_DOWN) {
                        focused = std::min(rc - 1, focused + columns);
                    } else if (vpad.trigger & VPAD_BUTTON_UP) {
                        focused = std::max(0, focused - columns);
                    }
                }
            }
        }

        if (inGame && !coreMenu) {
            core.runFrame();
        }

        beginFrame(bg);

        if (inGame) {
            if (core.hasFrame()) {
                const bool rgb565 = core.pixelFormat() == RETRO_PIXEL_FORMAT_RGB565;
                frameTex = uploadFrame(frameTex, core.frameData(),
                                       static_cast<int>(core.frameWidth()),
                                       static_cast<int>(core.frameHeight()),
                                       static_cast<int>(core.framePitch()), rgb565);
                if (frameTex >= 0) {
                    drawTexture(frameTex, 0.0f, 0.0f, 1280.0f, 720.0f);
                }
            }
            if (coreMenu) {
                drawRect(0.0f, 0.0f, 1280.0f, 720.0f, Color{0, 0, 0, 170});
                drawRoundRect(390.0f, 190.0f, 500.0f, 340.0f, 18.0f, panel);
                text::drawText("MENU", 430.0f, 230.0f, 26, white);
                text::drawText("Continuar", 430.0f, 310.0f, 20,
                               coreMenuIndex == 0 ? accent : muted);
                text::drawText("Cerrar core", 430.0f, 360.0f, 20,
                               coreMenuIndex == 1 ? accent : muted);
                text::drawText("A  Aceptar    B  Volver", 430.0f, 440.0f, 15, muted);
            }
        } else {
            text::drawText("Biblioteca", 28.0f, 18.0f, 34, white);

            const int count = std::min(10, static_cast<int>(visible.size()));
            if (count == 0) {
                text::drawText("Sin ROMs", 100.0f, 340.0f, 24, white);
            } else {
                for (int i = 0; i < count; ++i) {
                    const int romIndex = visible[i];
                    const int col = i % columns, row = i / columns;
                    const float x = gridX + col * (cardW + gap);
                    const float y = gridY + row * (cardH + gap);
                    const bool sel = i == focused;
                    drawRoundRect(x - 3.0f, y - 3.0f, cardW + 6.0f, cardH + 6.0f, 15.0f,
                                  sel ? accent : Color{255, 255, 255, 44});
                    drawRoundRect(x, y, cardW, cardH, 12.0f, cardPanel);
                    drawTextureCover(getCover(romIndex), x + 2.0f, y + 2.0f, cardW - 4.0f, coverH - 2.0f);
                    drawRoundRect(x + 2.0f, y + coverH, cardW - 4.0f, cardH - coverH - 2.0f, 10.0f, cardFooter);
                    text::drawText(getTitle(romIndex).c_str(),
                                   x + 12.0f, y + coverH + 12.0f, 15, white);
                    text::drawText(RETROU_CORE_LABEL, x + 12.0f, y + coverH + 34.0f, 12, muted);
                    if (library.roms[romIndex].favorite) {
                        drawRoundRect(x + cardW - 22.0f, y + 10.0f, 14.0f, 14.0f, 7.0f, gold);
                    }
                }
            }
            if (sidebarOpen) {
                drawRoundRect(24.0f, 88.0f, 300.0f, 560.0f, 18.0f, panel);
                text::drawText("FILTRAR Y ORGANIZAR", 44.0f, 106.0f, 16, muted);

                const char *regionNames[] = {"Todas", "USA", "Europa", "Japón"};
                std::string labels[4] = {
                    "Buscar",
                    std::string("Ordenar por: ") + (sortMode == 0 ? "Nombre" : "Región"),
                    std::string("Región: ") + regionNames[regionFilter],
                    std::string("Favoritos: ") + (favoritesOnly ? "Sí" : "No"),
                };
                for (int i = 0; i < 4; ++i) {
                    const float y = 150.0f + i * 44.0f;
                    if (i == sidebarIndex) {
                        drawRoundRect(36.0f, y - 8.0f, 276.0f, 34.0f, 8.0f, Color{245, 217, 127, 40});
                    }
                    text::drawText(labels[i].c_str(), 50.0f, y, 15, (i == sidebarIndex) ? white : muted);
                }
            }
            if (sidebarOpen) {
                text::drawText("A  SELECCIONAR    B  CERRAR", 28.0f, 672.0f, 15, muted);
            } else {
                text::drawText("A  JUGAR    X  FAVORITO    +  FILTROS", 28.0f, 672.0f, 15, muted);
            }
        }

        endFrame();
    }

    return 0;
}
