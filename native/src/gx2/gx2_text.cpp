#include "gx2_text.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstdint>
#include <map>
#include <vector>

namespace retrou::gx2::text {

namespace {

FT_Library g_library = nullptr;
FT_Face g_face = nullptr;

struct GlyphKey {
    uint32_t codepoint;
    int size;
    bool operator<(const GlyphKey &other) const
    {
        if (codepoint != other.codepoint) {
            return codepoint < other.codepoint;
        }
        return size < other.size;
    }
};

std::map<GlyphKey, Texture> g_glyphs;

Texture rasterizeGlyph(uint32_t codepoint, int size)
{
    GlyphKey key{codepoint, size};
    auto found = g_glyphs.find(key);
    if (found != g_glyphs.end()) {
        return found->second;
    }

    FT_Set_Pixel_Sizes(g_face, 0, static_cast<FT_UInt>(size));
    FT_UInt index = FT_Get_Char_Index(g_face, codepoint);
    if (FT_Load_Glyph(g_face, index, FT_LOAD_DEFAULT) != 0 ||
        FT_Render_Glyph(g_face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
        return -1;
    }

    FT_Bitmap &bitmap = g_face->glyph->bitmap;
    const int w = static_cast<int>(bitmap.width);
    const int h = static_cast<int>(bitmap.rows);
    if (w <= 0 || h <= 0) {
        return -1;
    }

    std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) {
        const unsigned char *src = bitmap.buffer + y * bitmap.pitch;
        for (int x = 0; x < w; ++x) {
            const unsigned char alpha = src[x];
            uint8_t *dst = &rgba[static_cast<size_t>(y * w + x) * 4];
            dst[0] = 255;
            dst[1] = 255;
            dst[2] = 255;
            dst[3] = alpha;
        }
    }

    Texture texture = createTexture(w, h, rgba.data());
    g_glyphs[key] = texture;
    return texture;
}

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

void drawText(const char *text, float x, float y, int pixelSize, Color color)
{
    if (!text || !g_face) {
        return;
    }

    float penX = x;
    const char *cursor = text;
    uint32_t codepoint = 0;
    int advance = 0;
    while (decodeUtf8(cursor, codepoint, advance)) {
        cursor += advance;

        Texture glyph = rasterizeGlyph(codepoint, pixelSize);
        if (glyph < 0) {
            continue;
        }

        FT_Set_Pixel_Sizes(g_face, 0, static_cast<FT_UInt>(pixelSize));
        FT_UInt index = FT_Get_Char_Index(g_face, codepoint);
        if (FT_Load_Glyph(g_face, index, FT_LOAD_DEFAULT) == 0) {
            const int left = g_face->glyph->bitmap_left;
            const int top = g_face->glyph->bitmap_top;
            const int w = static_cast<int>(g_face->glyph->bitmap.width);
            const int h = static_cast<int>(g_face->glyph->bitmap.rows);
            const int penAdvance = static_cast<int>(g_face->glyph->advance.x >> 6);

            drawTextureTinted(glyph,
                              penX + left,
                              y + pixelSize - top,
                              static_cast<float>(w),
                              static_cast<float>(h),
                              color);
            penX += penAdvance;
        }
    }
}

} // namespace retrou::gx2::text
