#define STBI_NO_THREAD_LOCALS
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "gx2_image.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <vector>

namespace retrou::gx2::image {

static void roundCorners(unsigned char *pixels, int width, int height, int radius)
{
    for (int py = 0; py < radius; ++py) {
        for (int px = 0; px < radius; ++px) {
            const int dx = radius - px;
            const int dy = radius - py;
            if (dx * dx + dy * dy <= radius * radius) continue;
            const int ys[2] = {py, height - 1 - py};
            const int xs[2] = {px, width - 1 - px};
            for (int y : ys) {
                for (int x : xs) {
                    unsigned char *p = pixels + (static_cast<size_t>(y) * width + x) * 4;
                    p[3] = 0;
                }
            }
        }
    }
}

Texture loadFromMemory(const unsigned char *data, size_t size)
{
    int width = 0, height = 0, channels = 0;
    unsigned char *pixels = stbi_load_from_memory(
        data, static_cast<int>(size), &width, &height, &channels, 4);
    if (!pixels) {
        return -1;
    }
    Texture texture = createTexture(width, height, pixels);
    stbi_image_free(pixels);
    return texture;
}

Texture loadFromMemoryRounded(const unsigned char *data, size_t size)
{
    int width = 0, height = 0, channels = 0;
    unsigned char *pixels = stbi_load_from_memory(
        data, static_cast<int>(size), &width, &height, &channels, 4);
    if (!pixels) {
        return -1;
    }
    const int radius = width > 40 ? std::max(4, width * 12 / 235) : 0;
    if (radius > 0 && height > radius * 2) {
        roundCorners(pixels, width, height, radius);
    }
    Texture texture = createTexture(width, height, pixels);
    stbi_image_free(pixels);
    return texture;
}

Texture loadFromFileRounded(const char *path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return -1;
    }
    std::vector<unsigned char> data(
        (std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return loadFromMemoryRounded(data.data(), data.size());
}

Texture loadFromFile(const char *path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return -1;
    }
    std::vector<unsigned char> data(
        (std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return loadFromMemory(data.data(), data.size());
}

} // namespace retrou::gx2::image
