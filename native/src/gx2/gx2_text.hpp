#pragma once

#include <cstddef>
#include <string>

#include "gx2_renderer.hpp"

namespace retrou::gx2::text {

// Loads a TrueType font from memory. Call after gx2::init(). Returns false on
// failure.
bool loadFont(const unsigned char *data, size_t size);

// Width in pixels of the UTF-8 string at the given pixel size.
int measureText(const char *text, int pixelSize);

// Returns `text` if it fits within maxWidth, otherwise a version truncated
// with "..." so it fits.
std::string fitText(const char *text, int pixelSize, float maxWidth);

// Draws UTF-8 text with its top-left at (x, y). Glyphs are rasterised with
// FreeType, cached per (codepoint, pixel size) and drawn as tinted textures.
void drawText(const char *text, float x, float y, int pixelSize, Color color);

// Creates the cached texture for a string ahead of the render loop. Textures
// must not be created mid-frame (it invalidates pending draws), so call this
// for every string the UI will render.
void prewarm(const char *text, int pixelSize);

} // namespace retrou::gx2::text
