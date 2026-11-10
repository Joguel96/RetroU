#pragma once

#include <cstddef>

#include "gx2_renderer.hpp"

namespace retrou::gx2::text {

// Loads a TrueType font from memory. Call after gx2::init(). Returns false on
// failure.
bool loadFont(const unsigned char *data, size_t size);

// Draws UTF-8 text with its top-left at (x, y). Glyphs are rasterised with
// FreeType, cached per (codepoint, pixel size) and drawn as tinted textures.
void drawText(const char *text, float x, float y, int pixelSize, Color color);

} // namespace retrou::gx2::text
