#include "gx2/gx2_renderer.hpp"

// Minimal GX2 diagnostic: clear to bright red and draw one white rectangle.
// No text, textures or input, to isolate clear/present from the rest.

int main()
{
    using namespace retrou::gx2;

    if (!init()) {
        return 1;
    }

    const Color red{255, 0, 0, 255};
    const Color white{255, 255, 255, 255};

    while (true) {
        beginFrame(red);
        drawRect(200.0f, 150.0f, 880.0f, 420.0f, white);
        endFrame();
    }

    return 0;
}
