#pragma once

#include <cstddef>

#include "gx2_renderer.hpp"

namespace retrou::gx2::image {

// Decodes an image (PNG/JPEG) from memory into an RGBA8 GX2 texture.
// Returns -1 on failure.
Texture loadFromMemory(const unsigned char *data, size_t size);
// Same as loadFromMemory/loadFromFile but rounds the corners (alpha) so the
// cover respects the card mask.
Texture loadFromMemoryRounded(const unsigned char *data, size_t size);
Texture loadFromFileRounded(const char *path);

// Decodes an image file into an RGBA8 GX2 texture. Returns -1 on failure.
Texture loadFromFile(const char *path);

} // namespace retrou::gx2::image
