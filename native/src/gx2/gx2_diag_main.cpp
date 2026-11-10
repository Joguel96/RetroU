#include "gx2/gx2_renderer.hpp"

#include <cstdint>
#include <vector>

// GX2 texture diagnostic: red background, a known-good white rect and a large
// procedural checkerboard texture. Isolates texture creation/drawing.

using namespace retrou::gx2;

static Texture makeChecker()
{
    constexpr int size = 64;
    std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool on = ((x / 8) + (y / 8)) % 2 == 0;
            uint8_t *px = &pixels[static_cast<size_t>(y * size + x) * 4];
            px[0] = on ? 255 : 46;   // R
            px[1] = on ? 217 : 92;   // G
            px[2] = on ? 127 : 138;  // B
            px[3] = 255;             // A
        }
    }
    return createTexture(size, size, pixels.data());
}

int main()
{
    if (!init()) {
        return 1;
    }

    const Color red{255, 0, 0, 255};
    const Color white{255, 255, 255, 255};
    Texture checker = makeChecker();

    while (true) {
        beginFrame(red);
        drawRect(40.0f, 40.0f, 200.0f, 100.0f, white);
        drawTextureCover(checker, 300.0f, 150.0f, 880.0f, 420.0f);
        endFrame();
    }

    return 0;
}
