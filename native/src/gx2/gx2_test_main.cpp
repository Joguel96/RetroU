#include "gx2/gx2_renderer.hpp"

#include <cstdint>
#include <vector>

// Smoke test for the GX2 renderer: fills the single TV buffer and draws
// gradients, rounded rectangles and a procedural texture every frame. If the
// TV-to-DRC copy works, the same image appears on the GamePad without a
// second render pass.

static retrou::gx2::Texture makeCheckerboard()
{
    constexpr int size = 64;
    std::vector<uint32_t> pixels(size * size);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool on = ((x / 8) + (y / 8)) % 2 == 0;
            pixels[y * size + x] = on ? 0xFFFFD97F : 0xFF2E5C8A;
        }
    }
    return retrou::gx2::createTexture(size, size, pixels.data());
}

int main()
{
    using namespace retrou::gx2;

    if (!init()) {
        return 1;
    }

    Texture checker = makeCheckerboard();

    Color background{18, 22, 30, 255};
    Color top{30, 120, 200, 255};
    Color bottom{12, 30, 60, 255};
    Color accent{240, 210, 120, 255};

    while (true) {
        beginFrame(background);

        drawGradient(0.0f, 0.0f, 1280.0f, 720.0f, top, bottom);
        drawRect(120.0f, 120.0f, 320.0f, 200.0f, accent);
        drawRoundRect(460.0f, 120.0f, 360.0f, 240.0f, 40.0f, Color{255, 0, 0, 220});
        drawTexture(checker, 500.0f, 400.0f, 200.0f, 160.0f);
        drawTextureCover(checker, 760.0f, 400.0f, 240.0f, 200.0f);

        endFrame();
    }

    return 0;
}
