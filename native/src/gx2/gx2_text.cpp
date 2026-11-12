#include "gx2_text.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace retrou::gx2::text {

namespace {

constexpr int kAtlasSize = 1024;

FT_Library g_library = nullptr;
FT_Face g_face = nullptr;

Texture g_atlas = -1;
std::vector<uint8_t> g_atlasPixels; // kAtlasSize * kAtlasSize * 4
int g_cursorX = 0;
int g_cursorY = 0;
int g_rowHeight = 0;
bool g_atlasDirty = false;

struct GlyphKey {
    uint32_t codepoint;
    int size;
    bool operator<(const GlyphKey &other) const
    {
        if (size != other.size) return size < other.size;
        return codepoint < other.codepoint;
    }
};

struct Glyph {
    float u0, v0, u1, v1;
    int width, height;
    int advance;
    int left, top;
};

std::map<GlyphKey, Glyph> g_glyphs;

int g_lastSize = -1;
void setSize(int size)
{
    if (size != g_lastSize) {
        FT_Set_Pixel_Sizes(g_face, 0, static_cast<FT_UInt>(size));
        g_lastSize = size;
    }
}

bool decodeUtf8(const char *text, uint32_t &out, int &advance)
{
    const unsigned char *p = reinterpret_cast<const unsigned char *>(text);
    if (p[0] == 0) return false;
    if (p[0] < 0x80) {
        out = p[0]; advance = 1;
    } else if ((p[0] & 0xE0) == 0xC0) {
        out = (p[0] & 0x1F) << 6 | (p[1] & 0x3F); advance = 2;
    } else if ((p[0] & 0xF0) == 0xE0) {
        out = (p[0] & 0x0F) << 12 | (p[1] & 0x3F) << 6 | (p[2] & 0x3F); advance = 3;
    } else {
        out = (p[0] & 0x07) << 18 | (p[1] & 0x3F) << 12 |
              (p[2] & 0x3F) << 6 | (p[3] & 0x3F); advance = 4;
    }
    return true;
}

Glyph renderGlyph(uint32_t codepoint, int size)
{
    GlyphKey key{codepoint, size};
    auto found = g_glyphs.find(key);
    if (found != g_glyphs.end()) {
        return found->second;
    }

    setSize(size);
    FT_UInt index = FT_Get_Char_Index(g_face, codepoint);
    if (FT_Load_Glyph(g_face, index, FT_LOAD_DEFAULT) != 0 ||
        FT_Render_Glyph(g_face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
        return Glyph{};
    }

    const FT_Bitmap &bitmap = g_face->glyph->bitmap;
    const int w = static_cast<int>(bitmap.width);
    const int h = static_cast<int>(bitmap.rows);
    if (w <= 0 || h <= 0) {
        Glyph g{};
        g.advance = static_cast<int>(g_face->glyph->advance.x >> 6);
        g.left = g_face->glyph->bitmap_left;
        g.top = g_face->glyph->bitmap_top;
        g_glyphs[key] = g;
        return g;
    }

    const int pad = 1;
    if (g_cursorX + w + pad > kAtlasSize) {
        g_cursorX = 0;
        g_cursorY += g_rowHeight;
        g_rowHeight = 0;
    }
    if (g_cursorY + h + pad > kAtlasSize) {
        // Atlas full: degrade gracefully by returning the glyph without a
        // texture (draws nothing). In practice the atlas is far from full.
        Glyph g{};
        g.advance = static_cast<int>(g_face->glyph->advance.x >> 6);
        g.left = g_face->glyph->bitmap_left;
        g.top = g_face->glyph->bitmap_top;
        g_glyphs[key] = g;
        return g;
    }

    const int dstX = g_cursorX;
    const int dstY = g_cursorY;
    for (int gy = 0; gy < h; ++gy) {
        const unsigned char *src = bitmap.buffer + gy * bitmap.pitch;
        for (int gx = 0; gx < w; ++gx) {
            uint8_t *px = &g_atlasPixels[
                (static_cast<size_t>(dstY + gy) * kAtlasSize + dstX + gx) * 4];
            px[0] = 255;
            px[1] = 255;
            px[2] = 255;
            px[3] = src[gx];
        }
    }

    Glyph g{};
    g.u0 = static_cast<float>(dstX) / kAtlasSize;
    g.v0 = static_cast<float>(dstY) / kAtlasSize;
    g.u1 = static_cast<float>(dstX + w) / kAtlasSize;
    g.v1 = static_cast<float>(dstY + h) / kAtlasSize;
    g.width = w;
    g.height = h;
    g.advance = static_cast<int>(g_face->glyph->advance.x >> 6);
    g.left = g_face->glyph->bitmap_left;
    g.top = g_face->glyph->bitmap_top;

    g_glyphs[key] = g;
    g_atlasDirty = true;

    g_cursorX += w + pad;
    g_rowHeight = std::max(g_rowHeight, h + pad);
    return g;
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
    g_atlasPixels.assign(static_cast<size_t>(kAtlasSize) * kAtlasSize * 4, 0);
    g_atlas = createTexture(kAtlasSize, kAtlasSize, g_atlasPixels.data());
    return g_atlas >= 0;
}

int measureText(const char *text, int pixelSize)
{
    if (!text || !g_face) return 0;
    setSize(pixelSize);
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
    if (!text) return {};
    if (measureText(text, pixelSize) <= maxWidth) return text;
    std::string result = text;
    while (!result.empty() &&
           measureText((result + "...").c_str(), pixelSize) > maxWidth) {
        // Remove a whole UTF-8 codepoint so the next measurement stays valid.
        while (!result.empty()) {
            const unsigned char removed = static_cast<unsigned char>(result.back());
            result.pop_back();
            if ((removed & 0xC0) != 0x80) break;
        }
    }
    return result + "...";
}

void prewarm(const char *text, int pixelSize)
{
    if (!text || !g_face || text[0] == 0) return;
    const char *cursor = text;
    uint32_t codepoint = 0;
    int advance = 0;
    while (decodeUtf8(cursor, codepoint, advance)) {
        cursor += advance;
        renderGlyph(codepoint, pixelSize);
    }
    if (g_atlasDirty && g_atlas >= 0) {
        updateTexture(g_atlas, g_atlasPixels.data());
        g_atlasDirty = false;
    }
}

void drawText(const char *text, float x, float y, int pixelSize, Color color)
{
    if (!text || !g_face || text[0] == 0) return;

    float penX = x;
    const char *cursor = text;
    uint32_t codepoint = 0;
    int advance = 0;
    while (decodeUtf8(cursor, codepoint, advance)) {
        cursor += advance;
        Glyph glyph = renderGlyph(codepoint, pixelSize);
        if (glyph.width > 0 && glyph.height > 0) {
            drawTexturedRect(g_atlas,
                             penX + glyph.left,
                             y + pixelSize - glyph.top,
                             static_cast<float>(glyph.width),
                             static_cast<float>(glyph.height),
                             glyph.u0, glyph.v0, glyph.u1, glyph.v1, color);
        }
        penX += glyph.advance;
    }

    if (g_atlasDirty && g_atlas >= 0) {
        updateTexture(g_atlas, g_atlasPixels.data());
        g_atlasDirty = false;
    }
}

} // namespace retrou::gx2::text
