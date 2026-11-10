#include "gx2/gx2_renderer.hpp"
#include "gx2/gx2_text.hpp"
#include "gx2/gx2_image.hpp"

#include <vpad/input.h>

#include <cstdint>
#include <vector>

// GX2 UI visual test: renders a representative library screen (header,
// sidebar, grid of rounded cards with procedural covers and metadata) every
// frame from the single TV buffer, which is then copied to the GamePad.

extern const unsigned char _binary_ui_ttf_start[];
extern const unsigned char _binary_ui_ttf_end[];

namespace {

using retrou::gx2::Color;
using retrou::gx2::Texture;

struct SampleGame {
    const char *title;
    const char *meta;
    uint32_t top;
    uint32_t bottom;
};

const SampleGame kGames[] = {
    {"Castlevania", "Konami · 1986", 0xFF2E3A59, 0xFF0D1220},
    {"Super Mario Bros.", "Nintendo · 1985", 0xFF1F6E3F, 0xFF0A2415},
    {"The Legend of Zelda", "Nintendo · 1986", 0xFF4A5A20, 0xFF181F0A},
    {"Metroid", "Nintendo · 1986", 0xFF3B2E5A, 0xFF150F24},
    {"Mega Man 2", "Capcom · 1988", 0xFF20445A, 0xFF0A1B24},
    {"Kirby's Adventure", "HAL · 1993", 0xFF6E3F6E, 0xFF251225},
    {"DuckTales", "Capcom · 1989", 0xFF5A4A20, 0xFF1F180A},
    {"Contra", "Konami · 1988", 0xFF5A2020, 0xFF200A0A},
    {"Final Fantasy", "Square · 1987", 0xFF20205A, 0xFF0A0A20},
    {"Punch-Out!!", "Nintendo · 1987", 0xFF4A5A5A, 0xFF182020},
};

Texture makeCover(uint32_t top, uint32_t bottom)
{
    constexpr int w = 96;
    constexpr int h = 128;
    std::vector<uint32_t> pixels(w * h);
    for (int y = 0; y < h; ++y) {
        const float t = static_cast<float>(y) / (h - 1);
        const uint8_t tr = static_cast<uint8_t>(((top >> 24) & 0xFF) * (1.0f - t) +
                                                 ((bottom >> 24) & 0xFF) * t);
        const uint8_t tg = static_cast<uint8_t>(((top >> 16) & 0xFF) * (1.0f - t) +
                                                 ((bottom >> 16) & 0xFF) * t);
        const uint8_t tb = static_cast<uint8_t>(((top >> 8) & 0xFF) * (1.0f - t) +
                                                 ((bottom >> 8) & 0xFF) * t);
        const uint8_t ta = static_cast<uint8_t>(((top >> 0) & 0xFF) * (1.0f - t) +
                                                 ((bottom >> 0) & 0xFF) * t);
        const uint32_t px = (tr << 24) | (tg << 16) | (tb << 8) | ta;
        for (int x = 0; x < w; ++x) {
            pixels[y * w + x] = px;
        }
    }
    return retrou::gx2::createTexture(w, h, pixels.data());
}

} // namespace

int main()
{
    using namespace retrou::gx2;

    if (!init()) {
        return 1;
    }
    if (!text::loadFont(_binary_ui_ttf_start,
                        static_cast<size_t>(_binary_ui_ttf_end - _binary_ui_ttf_start))) {
        return 1;
    }

    Texture covers[10];
    for (int i = 0; i < 10; ++i) {
        covers[i] = makeCover(kGames[i].top, kGames[i].bottom);
    }

    // If the user has a downloaded cover on the SD, use it for the first card
    // to exercise real PNG decoding; otherwise keep the procedural cover.
    Texture realCover = image::loadFromFile(
        "/vol/external01/retrou/thumbnails/nes/Castlevania (USA).png");

    const Color bgTop{20, 28, 44, 255};
    const Color bgBottom{8, 11, 18, 255};
    const Color accent{245, 217, 127, 255};
    const Color white{255, 255, 255, 255};
    const Color muted{148, 163, 184, 255};
    const Color panel{21, 27, 40, 235};
    const Color cardPanel{16, 21, 33, 235};
    const Color cardFooter{10, 14, 24, 235};

    const float sidebarX = 24.0f;
    const float sidebarY = 88.0f;
    const float sidebarW = 300.0f;
    const float sidebarH = 560.0f;
    const float gridX = 344.0f;
    const float gridY = 88.0f;
    const float gridW = 1280.0f - gridX - 28.0f;
    const float gridH = 560.0f;
    const float gap = 14.0f;
    const int columns = 5;
    const int rows = 2;
    const float cardW = (gridW - gap * (columns - 1)) / columns;
    const float cardH = (gridH - gap * (rows - 1)) / rows;
    const float coverH = cardH * 0.62f;

    const char *filters[] = {
        "Buscar: Todos",
        "Ordenar por: Nombre",
        "Región: Todas",
        "Favoritos: No",
    };

    int focused = 0;

    while (true) {
        // GamePad input: move the focused card with the D-pad.
        VPADStatus vpad{};
        VPADReadError vpadError = VPAD_READ_NO_SAMPLES;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &vpadError);
        if (vpadError == VPAD_READ_SUCCESS) {
            if (vpad.trigger & VPAD_BUTTON_RIGHT) focused = (focused + 1) % 10;
            if (vpad.trigger & VPAD_BUTTON_LEFT) focused = (focused + 9) % 10;
            if (vpad.trigger & VPAD_BUTTON_DOWN) focused = (focused + 5) % 10;
            if (vpad.trigger & VPAD_BUTTON_UP) focused = (focused + 5) % 10;
        }

        beginFrame(Color{8, 11, 18, 255});

        // Background.
        drawGradient(0.0f, 0.0f, 1280.0f, 720.0f, bgTop, bgBottom);

        // Header title.
        text::drawText("Biblioteca", 28.0f, 18.0f, 34, white);

        // Sidebar.
        drawRoundRect(sidebarX, sidebarY, sidebarW, sidebarH, 18.0f, panel);
        text::drawText("FILTRAR Y ORGANIZAR", sidebarX + 20.0f, sidebarY + 18.0f, 16, muted);
        for (int i = 0; i < 4; ++i) {
            const float y = sidebarY + 58.0f + i * 44.0f;
            if (i == 0) {
                drawRoundRect(sidebarX + 12.0f, y - 8.0f, sidebarW - 24.0f, 34.0f, 8.0f,
                              Color{245, 217, 127, 40});
            }
            text::drawText(filters[i], sidebarX + 26.0f, y, 15, i == 0 ? white : muted);
        }

        // Card grid.
        for (int i = 0; i < 10; ++i) {
            const int col = i % columns;
            const int row = i / columns;
            const float x = gridX + col * (cardW + gap);
            const float y = gridY + row * (cardH + gap);
            const bool isFocused = i == focused;

            // Focus ring + card panel (ring = accent behind, panel on top).
            drawRoundRect(x - 3.0f, y - 3.0f, cardW + 6.0f, cardH + 6.0f, 15.0f,
                          isFocused ? accent : Color{255, 255, 255, 44});
            drawRoundRect(x, y, cardW, cardH, 12.0f, cardPanel);

            // Cover (inset), then footer below it.
            Texture cover = (i == 0 && realCover >= 0) ? realCover : covers[i];
            drawTextureCover(cover, x + 2.0f, y + 2.0f, cardW - 4.0f, coverH - 2.0f);
            drawRoundRect(x + 2.0f, y + coverH, cardW - 4.0f, cardH - coverH - 2.0f, 10.0f,
                          cardFooter);

            text::drawText(text::fitText(kGames[i].title, 15, cardW - 24.0f).c_str(),
                           x + 12.0f, y + coverH + 12.0f, 15, white);
            text::drawText(kGames[i].meta, x + 12.0f, y + coverH + 34.0f, 12, muted);
        }

        // Footer hints.
        text::drawText("A  JUGAR    B  VOLVER    PLUS  FILTROS", 28.0f, 672.0f, 15, muted);

        endFrame();
    }

    return 0;
}
