#pragma once

// RetroU GX2 renderer.
//
// Single-render architecture: the whole UI (and the core game frame) is drawn
// once into a 1280x720 TV color buffer. At endFrame() that same buffer is
// copied to the TV scan buffer AND to the DRC (GamePad) scan buffer, so there
// is only one render pass and one set of UI textures — no second renderer and
// no duplicated framebuffer for the GamePad.
//
// The GamePad receives a scaled copy of the TV image via the hardware scaler,
// which is cheaper in memory and CPU than maintaining a separate 854x480
// render target and re-drawing the scene for it.

#include <cstdint>

namespace retrou::gx2 {

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

// Owned texture handle returned by createTexture().
using Texture = int;

// Initialises GX2 and the single TV color buffer. Returns false on failure.
bool init();

// Frees GPU resources.
void shutdown();

// Begins a frame: clears the color buffer to `clear` and starts drawing to TV.
void beginFrame(Color clear);

// Finishes a frame: copies the TV buffer to TV + DRC scan buffers and swaps.
void endFrame();

// Draws an axis-aligned, screen-space rectangle (pixels, top-left origin).
void drawRect(float x, float y, float w, float h, Color color);

// Draws a rounded rectangle with the given corner radius (pixels).
void drawRoundRect(float x, float y, float w, float h, float radius, Color color);

// Draws a vertical linear gradient between two colors.
void drawGradient(float x, float y, float w, float h, Color top, Color bottom);

// Creates a texture from RGBA8 pixels (top-left origin). Returns >=0 or -1.
Texture createTexture(int width, int height, const void *rgba);

void destroyTexture(Texture texture);

// Draws a texture into a destination rectangle (pixels).
void drawTexture(Texture texture, float x, float y, float w, float h);

// Draws a texture preserving its aspect ratio inside the destination box,
// cropping (cover) instead of stretching.
void drawTextureCover(Texture texture, float x, float y, float w, float h);

// Restricts drawing to a rectangular region.
void setScissor(int x, int y, int w, int h);
void clearScissor();

} // namespace retrou::gx2
