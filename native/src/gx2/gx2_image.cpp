#define STBI_NO_THREAD_LOCALS
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "gx2_image.hpp"

#include <fstream>
#include <iterator>
#include <vector>

namespace retrou::gx2::image {

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
