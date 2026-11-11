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

Texture loadFromFileBlurred(const char *path, int smallW, int smallH)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return -1;
    }
    std::vector<unsigned char> data(
        (std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    int w = 0, h = 0, ch = 0;
    unsigned char *pixels = stbi_load_from_memory(
        data.data(), static_cast<int>(data.size()), &w, &h, &ch, 4);
    if (!pixels || w <= 0 || h <= 0) {
        return -1;
    }
    std::vector<unsigned char> small(static_cast<size_t>(smallW) * smallH * 4, 0);
    for (int y = 0; y < smallH; ++y) {
        const int sy0 = y * h / smallH;
        const int sy1 = std::max(sy0 + 1, (y + 1) * h / smallH);
        for (int x = 0; x < smallW; ++x) {
            const int sx0 = x * w / smallW;
            const int sx1 = std::max(sx0 + 1, (x + 1) * w / smallW);
            int r = 0, g = 0, b = 0, a = 0, n = 0;
            for (int sy = sy0; sy < sy1 && sy < h; ++sy) {
                for (int sx = sx0; sx < sx1 && sx < w; ++sx) {
                    const unsigned char *p = pixels + (static_cast<size_t>(sy) * w + sx) * 4;
                    r += p[0]; g += p[1]; b += p[2]; a += p[3]; ++n;
                }
            }
            if (n == 0) n = 1;
            unsigned char *q = &small[(static_cast<size_t>(y) * smallW + x) * 4];
            q[0] = static_cast<unsigned char>(r / n);
            q[1] = static_cast<unsigned char>(g / n);
            q[2] = static_cast<unsigned char>(b / n);
            q[3] = static_cast<unsigned char>(a / n);
        }
    }
    stbi_image_free(pixels);
    return createTexture(smallW, smallH, small.data());
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
