#include "gx2/gx2_renderer.hpp"

#include <vpad/input.h>

// Incremental GX2 diagnostic: sidebar + 5x2 grid of rounded card panels
// (solid only, no textures or text). If this holds 60 FPS, the next step adds
// cover textures, then text, to find what causes the slowdown.

using namespace retrou::gx2;

int main()
{
    if (!init()) {
        return 1;
    }

    const Color bg{8, 11, 18, 255};
    const Color accent{245, 217, 127, 255};
    const Color panel{28, 34, 48, 255};
    const Color panelAlt{38, 44, 60, 255};

    const int columns = 5;
    const int rows = 2;
    const float gridX = 344.0f;
    const float gridY = 88.0f;
    const float gridW = 1280.0f - gridX - 28.0f;
    const float gridH = 560.0f;
    const float gap = 14.0f;
    const float cardW = (gridW - gap * (columns - 1)) / columns;
    const float cardH = (gridH - gap * (rows - 1)) / rows;

    while (true) {
        VPADStatus vpad{};
        VPADReadError vpadError = VPAD_READ_NO_SAMPLES;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &vpadError);

        beginFrame(bg);

        drawRoundRect(24.0f, 88.0f, 300.0f, 560.0f, 18.0f, panel);

        for (int i = 0; i < 10; ++i) {
            const int col = i % columns;
            const int row = i / columns;
            const float x = gridX + col * (cardW + gap);
            const float y = gridY + row * (cardH + gap);
            if (i == 0) {
                drawRoundRect(x - 3.0f, y - 3.0f, cardW + 6.0f, cardH + 6.0f, 15.0f, accent);
            }
            drawRoundRect(x, y, cardW, cardH, 12.0f, (i % 2) ? panelAlt : panel);
        }

        endFrame();
    }

    return 0;
}
