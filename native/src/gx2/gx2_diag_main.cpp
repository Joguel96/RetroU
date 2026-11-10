#include "gx2/gx2_renderer.hpp"
#include "gx2/gx2_text.hpp"

#include <vpad/input.h>

#include <cstdint>
#include <vector>

// Incremental GX2 diagnostic: sidebar + 5x2 card panels + cover textures +
// card title text. This is the last step before the full UI: it should show
// whether text rendering is what tanks the frame rate.

using namespace retrou::gx2;

extern const unsigned char _binary_ui_ttf_start[];
extern const unsigned char _binary_ui_ttf_end[];

static Texture makeCover(uint8_t r0, uint8_t g0, uint8_t b0,
                         uint8_t r1, uint8_t g1, uint8_t b1)
{
    constexpr int w = 96;
    constexpr int h = 128;
    std::vector<uint8_t> pixels(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) {
        const float t = static_cast<float>(y) / (h - 1);
        const uint8_t r = static_cast<uint8_t>(r0 * (1.0f - t) + r1 * t);
        const uint8_t g = static_cast<uint8_t>(g0 * (1.0f - t) + g1 * t);
        const uint8_t b = static_cast<uint8_t>(b0 * (1.0f - t) + b1 * t);
        for (int x = 0; x < w; ++x) {
            uint8_t *px = &pixels[(static_cast<size_t>(y) * w + x) * 4];
            px[0] = r;
            px[1] = g;
            px[2] = b;
            px[3] = 255;
        }
    }
    return createTexture(w, h, pixels.data());
}

int main()
{
    if (!init()) {
        return 1;
    }
    if (!text::loadFont(_binary_ui_ttf_start,
                        static_cast<size_t>(_binary_ui_ttf_end - _binary_ui_ttf_start))) {
        return 1;
    }

    // Pre-create text textures outside the render loop.
    text::prewarm("Castlevania", 15);
    text::prewarm("NES", 12);

    const Color bg{8, 11, 18, 255};
    const Color accent{245, 217, 127, 255};
    const Color panel{28, 34, 48, 255};
    const Color panelAlt{38, 44, 60, 255};

    Texture covers[10];
    for (int i = 0; i < 10; ++i) {
        covers[i] = makeCover(static_cast<uint8_t>(30 + i * 12),
                              static_cast<uint8_t>(60 + i * 8),
                              static_cast<uint8_t>(120 - i * 5),
                              10, 14, 24);
    }

    // Synthetic RGB565 frame to validate uploadFrame (the core frame path).
    std::vector<uint16_t> framePixels(256 * 144);
    for (int y = 0; y < 144; ++y) {
        for (int x = 0; x < 256; ++x) {
            const uint16_t r5 = static_cast<uint16_t>(x >> 3);
            const uint16_t g6 = static_cast<uint16_t>((y * 2) >> 2);
            const uint16_t b5 = 16;
            framePixels[y * 256 + x] =
                static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
        }
    }
    Texture frame = uploadFrame(-1, framePixels.data(), 256, 144, 256 * 2, true);

    const int columns = 5;
    const int rows = 2;
    const float gridX = 344.0f;
    const float gridY = 88.0f;
    const float gridW = 1280.0f - gridX - 28.0f;
    const float gridH = 560.0f;
    const float gap = 14.0f;
    const float cardW = (gridW - gap * (columns - 1)) / columns;
    const float cardH = (gridH - gap * (rows - 1)) / rows;
    const float coverH = cardH * 0.62f;

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
            drawTextureCover(covers[i], x + 2.0f, y + 2.0f, cardW - 4.0f, coverH - 2.0f);
            text::drawText("Castlevania", x + 12.0f, y + coverH + 12.0f, 15,
                           Color{255, 255, 255, 255});
            text::drawText("NES", x + 12.0f, y + coverH + 34.0f, 12,
                           Color{148, 163, 184, 255});
        }

        // Synthetic core frame (RGB565) to verify the frame upload path.
        if (frame >= 0) {
            drawTexture(frame, 344.0f, 656.0f, 400.0f, 60.0f);
        }

        endFrame();
    }

    return 0;
}
