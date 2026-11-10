#include "gx2_text.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace retrou::gx2::text {

namespace {

FT_Library g_library = nullptr;
FT_Face g_face = nullptr;

struct StringKey {
    std::string text;
    int size;
    bool operator<(const StringKey &other) const
    {
        if (size != other.size) {
            return size < other.size;
        }
        return text < other.text;
    }
};

struct StringSprite {
    Texture texture = -1;
    int width = 0;
    int height = 0;
};

std::map<StringKey, StringSprite> g_strings;

bool decodeUtf8(const char *text, uint32_t &out, int &advance)
{
    const unsigned char *p = reinterpret_cast<const unsigned char *>(text);
    if (p[0] == 0) {
        return false;
    }
    if (p[0] < 0x80) {
        out = p[0];
        advance = 1;
    } else if ((p[0] & 0xE0) == 0xC0) {
        out = (p[0] & 0x1F) << 6 | (p[1] & 0x3F);
        advance = 2;
    } else if ((p[0] & 0xF0) == 0xE0) {
        out = (p[0] & 0x0F) << 12 | (p[1] & 0x3F) << 6 | (p[2] & 0x3F);
        advance = 3;
    } else {
        out = (p[0] & 0x07) << 18 | (p[1] & 0x3F) << 12 |
              (p[2] & 0x3F) << 6 | (p[3] & 0x3F);
        advance = 4;
    }
    return true;
}

// Renders a whole string into one RGBA texture (white glyphs + alpha). Drawing
// a string then costs a single textured quad instead of one draw per glyph,
// which is what keeps the GX2 frontend at 60 FPS.
StringSprite renderString(const std::string &text, int size)
{
    StringSprite sprite;
    if (!g_face || text.empty() || size <= 0) {
        return sprite;
    }

    FT_Set_Pixel_Sizes(g_face, 0, static_cast<FT_UInt>(size));
    const int ascent = static_cast<int>(g_face->size->metrics.ascender >> 6);
    const int descent = static_cast<int>(-(g_face->size->metrics.descender >> 6));

    // First pass: total advance width.
    int width = 0;
    {
        const char *cursor = text.c_str();
        uint32_t codepoint = 0;
        int advance = 0;
        while (decodeUtf8(cursor, codepoint, advance)) {
            cursor += advance;
            FT_UInt index = FT_Get_Char_Index(g_face, codepoint);
            if (FT_Load_Glyph(g_face, index, FT_LOAD_DEFAULT) == 0) {
                width += static_cast<int>(g_face->glyph->advance.x >> 6);
            }
        }
    }
    if (width <= 0) {
        return sprite;
    }

    const int padding = 2;
    const int height = ascent + descent + padding;
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4, 0);

    // Second pass: rasterise and copy each glyph into the string bitmap.
    const char *cursor = text.c_str();
    uint32_t codepoint = 0;
    int advance = 0;
    int penX = 0;
    while (decodeUtf8(cursor, codepoint, advance)) {
        cursor += advance;
        FT_UInt index = FT_Get_Char_Index(g_face, codepoint);
        if (FT_Load_Glyph(g_face, index, FT_LOAD_DEFAULT) != 0 ||
            FT_Render_Glyph(g_face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
            continue;
        }
        const FT_Bitmap &bitmap = g_face->glyph->bitmap;
        const int gw = static_cast<int>(bitmap.width);
        const int gh = static_cast<int>(bitmap.rows);
        const int left = g_face->glyph->bitmap_left;
        const int top = g_face->glyph->bitmap_top;

        for (int gy = 0; gy < gh; ++gy) {
            const int dstY = ascent - top + gy;
            if (dstY < 0 || dstY >= height) {
                continue;
            }
            const unsigned char *src = bitmap.buffer + gy * bitmap.pitch;
            for (int gx = 0; gx < gw; ++gx) {
                const int dstX = penX + left + gx;
                if (dstX < 0 || dstX >= width) {
                    continue;
                }
                uint8_t *pixel = &pixels[(static_cast<size_t>(dstY) * width + dstX) * 4];
                pixel[0] = 255;
                pixel[1] = 255;
                pixel[2] = 255;
                pixel[3] = src[gx];
            }
        }
        penX += static_cast<int>(g_face->glyph->advance.x >> 6);
    }

    sprite.texture = createTexture(width, height, pixels.data());
    sprite.width = width;
    sprite.height = height;
    return sprite;
}

} // namespace

bool loadFont(const unsigned char *data, size_t size)
{
    if (g_face) {
        return true;
    }
    if (FT_Init_FreeType(&g_library) != 0) {
        return false;
    }
    if (FT_New_Memory_Face(g_library, data, static_cast<FT_Long>(size), 0,
                           &g_face) != 0) {
        return false;
    }
    return true;
}

int measureText(const char *text, int pixelSize)
{
    if (!text || !g_face) {
        return 0;
    }
    FT_Set_Pixel_Sizes(g_face, 0, static_cast<FT_UInt>(pixelSize));

    int width = 0;
    const char *cursor = text;
    uint32_t codepoint = 0;
    int advance = 0;
    while (decodeUtf8(cursor, codepoint, advance)) {
        cursor += advance;
        FT_UInt index = FT_Get_Char_Index(g_face, codepoint);
        if (FT_Load_Glyph(g_face, index, FT_LOAD_DEFAULT) == 0) {
            width += static_cast<int>(g_face->glyph->advance.x >> 6);
        }
    }
    return width;
}

std::string fitText(const char *text, int pixelSize, float maxWidth)
{
    if (!text) {
        return {};
    }
    if (measureText(text, pixelSize) <= maxWidth) {
        return text;
    }
    std::string result = text;
    while (!result.empty() &&
           measureText((result + "...").c_str(), pixelSize) > maxWidth) {
        result.pop_back();
    }
    return result + "...";
}

void drawText(const char *text, float x, float y, int pixelSize, Color color)
{
    if (!text || !g_face || text[0] == 0) {
        return;
    }

    StringKey key{text, pixelSize};
    auto found = g_strings.find(key);
    if (found == g_strings.end()) {
        StringSprite sprite = renderString(text, pixelSize);
        found = g_strings.emplace(key, sprite).first;
    }

    const StringSprite &sprite = found->second;
    if (sprite.texture < 0) {
        return;
    }
    drawTextureTinted(sprite.texture, x, y,
                      static_cast<float>(sprite.width),
                      static_cast<float>(sprite.height), color);
}

} // namespace retrou::gx2::text
