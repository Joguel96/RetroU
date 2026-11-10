#include "gx2/gx2_renderer.hpp"

// Smoke test for the GX2 renderer: fills the single TV buffer and draws a
// gradient and a solid rectangle every frame. If the TV-to-DRC copy works,
// the same image appears on the GamePad without a second render pass.

int main()
{
    using namespace retrou::gx2;

    if (!init()) {
        return 1;
    }

    Color background{18, 22, 30, 255};
    Color top{30, 120, 200, 255};
    Color bottom{12, 30, 60, 255};
    Color accent{240, 210, 120, 255};

    while (true) {
        beginFrame(background);

        drawGradient(0.0f, 0.0f, 1280.0f, 720.0f, top, bottom);
        drawRect(120.0f, 120.0f, 320.0f, 200.0f, accent);
        drawRoundRect(460.0f, 120.0f, 360.0f, 240.0f, 40.0f, Color{255, 0, 0, 220});
        drawRect(500.0f, 400.0f, 280.0f, 160.0f, Color{40, 200, 120, 255});

        endFrame();
    }

    return 0;
}
