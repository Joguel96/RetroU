#include "gx2_renderer.hpp"

#include <whb/gfx.h>
#include <gx2/clear.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/shaders.h>
#include <gx2/swap.h>
#include <gx2r/buffer.h>
#include <gx2r/draw.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace retrou::gx2 {

namespace {

// Logical resolution of the single render target (16:9). All UI coordinates
// are expressed in these units and mapped to normalized device coordinates,
// so the layout stays the same whether the TV is 720p or 1080p.
constexpr float kScreenW = 1280.0f;
constexpr float kScreenH = 720.0f;
constexpr float kPi = 3.14159265358979323846f;

#include "solid_gsh.inc"
#include "texture_gsh.inc"

WHBGfxShaderGroup g_solidGroup{};
WHBGfxShaderGroup g_textureGroup{};

GX2RBuffer g_posBuffer{};
GX2RBuffer g_colBuffer{};

std::vector<float> g_solidPos; // 2 floats per vertex
std::vector<float> g_solidCol; // 4 floats per vertex

bool g_ready = false;
bool g_inFrame = false;

void uploadBuffer(GX2RBuffer &buffer, const std::vector<float> &data)
{
    if (data.empty()) {
        return;
    }

    if (!buffer.buffer || buffer.elemCount < data.size()) {
        if (buffer.buffer) {
            GX2RDestroyBufferEx(&buffer, GX2R_RESOURCE_BIND_NONE);
        }
        std::memset(&buffer, 0, sizeof(buffer));
        buffer.flags = GX2R_RESOURCE_BIND_VERTEX_BUFFER |
                       GX2R_RESOURCE_USAGE_CPU_READ |
                       GX2R_RESOURCE_USAGE_CPU_WRITE |
                       GX2R_RESOURCE_USAGE_GPU_READ;
        buffer.elemSize = sizeof(float);
        buffer.elemCount = static_cast<uint32_t>(data.size());
        GX2RCreateBuffer(&buffer);
    }

    void *mapped = GX2RLockBufferEx(&buffer, GX2R_RESOURCE_BIND_NONE);
    if (mapped) {
        std::memcpy(mapped, data.data(), data.size() * sizeof(float));
        GX2RUnlockBufferEx(&buffer, GX2R_RESOURCE_BIND_NONE);
    }
}

void pushSolidVertex(float x, float y, Color c)
{
    const float ndcX = x / (kScreenW * 0.5f) - 1.0f;
    const float ndcY = 1.0f - y / (kScreenH * 0.5f);
    g_solidPos.push_back(ndcX);
    g_solidPos.push_back(ndcY);
    g_solidCol.push_back(c.r / 255.0f);
    g_solidCol.push_back(c.g / 255.0f);
    g_solidCol.push_back(c.b / 255.0f);
    g_solidCol.push_back(c.a / 255.0f);
}

void pushTriangle(float x0, float y0, float x1, float y1,
                  float x2, float y2, Color c)
{
    pushSolidVertex(x0, y0, c);
    pushSolidVertex(x1, y1, c);
    pushSolidVertex(x2, y2, c);
}

void pushQuad(float x0, float y0, float x1, float y1, Color c)
{
    pushTriangle(x0, y0, x1, y0, x1, y1, c);
    pushTriangle(x0, y0, x1, y1, x0, y1, c);
}

void pushGradientQuad(float x0, float y0, float x1, float y1,
                      Color top, Color bottom)
{
    pushSolidVertex(x0, y0, top);
    pushSolidVertex(x1, y0, top);
    pushSolidVertex(x1, y1, bottom);

    pushSolidVertex(x0, y0, top);
    pushSolidVertex(x1, y1, bottom);
    pushSolidVertex(x0, y1, bottom);
}

void pushCorner(float cx, float cy, float radius, float a0, float a1,
                Color c, int segments)
{
    for (int i = 0; i < segments; ++i) {
        float t0 = a0 + (a1 - a0) * i / segments;
        float t1 = a0 + (a1 - a0) * (i + 1) / segments;
        float p0x = cx + std::cos(t0) * radius;
        float p0y = cy + std::sin(t0) * radius;
        float p1x = cx + std::cos(t1) * radius;
        float p1y = cy + std::sin(t1) * radius;
        pushTriangle(cx, cy, p0x, p0y, p1x, p1y, c);
    }
}

void flushSolid()
{
    if (g_solidPos.empty()) {
        return;
    }

    uploadBuffer(g_posBuffer, g_solidPos);
    uploadBuffer(g_colBuffer, g_solidCol);

    GX2SetFetchShader(&g_solidGroup.fetchShader);
    GX2SetVertexShader(g_solidGroup.vertexShader);
    GX2SetPixelShader(g_solidGroup.pixelShader);
    GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_REGISTER);

    GX2RSetAttributeBuffer(&g_posBuffer, 0, 2 * sizeof(float), 0);
    GX2RSetAttributeBuffer(&g_colBuffer, 1, 4 * sizeof(float), 0);

    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES,
              static_cast<uint32_t>(g_solidPos.size() / 2), 0, 1);

    g_solidPos.clear();
    g_solidCol.clear();
}

} // namespace

bool init()
{
    if (g_ready) {
        return true;
    }

    if (!WHBGfxInit()) {
        return false;
    }

    if (!WHBGfxLoadGFDShaderGroup(&g_solidGroup, 0, solid_gsh)) {
        WHBGfxShutdown();
        return false;
    }
    if (!WHBGfxLoadGFDShaderGroup(&g_textureGroup, 0, texture_gsh)) {
        WHBGfxShutdown();
        return false;
    }

    // solid:   location 0 = aPos (vec2), location 1 = aColor (vec4)
    WHBGfxInitShaderAttribute(&g_solidGroup, "aPos", 0, 0,
                              GX2_ATTRIB_FORMAT_FLOAT_32_32);
    WHBGfxInitShaderAttribute(&g_solidGroup, "aColor", 1, 0,
                              GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
    WHBGfxInitFetchShader(&g_solidGroup);

    // texture: location 0 = aPos, location 1 = aUV, location 2 = aColor
    WHBGfxInitShaderAttribute(&g_textureGroup, "aPos", 0, 0,
                              GX2_ATTRIB_FORMAT_FLOAT_32_32);
    WHBGfxInitShaderAttribute(&g_textureGroup, "aUV", 1, 0,
                              GX2_ATTRIB_FORMAT_FLOAT_32_32);
    WHBGfxInitShaderAttribute(&g_textureGroup, "aColor", 2, 0,
                              GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
    WHBGfxInitFetchShader(&g_textureGroup);

    g_ready = true;
    return true;
}

void shutdown()
{
    if (!g_ready) {
        return;
    }
    WHBGfxFreeShaderGroup(&g_solidGroup);
    WHBGfxFreeShaderGroup(&g_textureGroup);
    if (g_posBuffer.buffer) {
        GX2RDestroyBufferEx(&g_posBuffer, GX2R_RESOURCE_BIND_NONE);
    }
    if (g_colBuffer.buffer) {
        GX2RDestroyBufferEx(&g_colBuffer, GX2R_RESOURCE_BIND_NONE);
    }
    WHBGfxShutdown();
    g_ready = false;
}

void beginFrame(Color clear)
{
    if (!g_ready) {
        return;
    }
    WHBGfxBeginRender();
    WHBGfxBeginRenderTV();
    WHBGfxClearColor(clear.r / 255.0f, clear.g / 255.0f,
                     clear.b / 255.0f, clear.a / 255.0f);
    g_inFrame = true;
}

void endFrame()
{
    if (!g_ready) {
        return;
    }
    flushSolid();
    // Single render pass: the one TV color buffer is copied to the TV scan
    // buffer and to the DRC (GamePad) scan buffer. No second render target.
    WHBGfxFinishRenderTV();
    GX2CopyColorBufferToScanBuffer(WHBGfxGetTVColourBuffer(), GX2_SCAN_TARGET_DRC);
    WHBGfxFinishRender();
    g_inFrame = false;
}

void drawRect(float x, float y, float w, float h, Color color)
{
    if (!g_ready || !g_inFrame) {
        return;
    }
    pushQuad(x, y, x + w, y + h, color);
}

void drawGradient(float x, float y, float w, float h, Color top, Color bottom)
{
    if (!g_ready || !g_inFrame) {
        return;
    }
    pushGradientQuad(x, y, x + w, y + h, top, bottom);
}

void drawRoundRect(float x, float y, float w, float h, float radius, Color color)
{
    if (!g_ready || !g_inFrame) {
        return;
    }
    if (radius <= 0.0f) {
        pushQuad(x, y, x + w, y + h, color);
        return;
    }

    const float r = std::min(radius, std::min(w, h) * 0.5f);
    const float x0 = x, y0 = y, x1 = x + w, y1 = y + h;
    const int segments = 8;

    // Center and the four edge rectangles.
    pushQuad(x0 + r, y0 + r, x1 - r, y1 - r, color); // center
    pushQuad(x0 + r, y0, x1 - r, y0 + r, color);     // top
    pushQuad(x0 + r, y1 - r, x1 - r, y1, color);     // bottom
    pushQuad(x0, y0 + r, x0 + r, y1 - r, color);     // left
    pushQuad(x1 - r, y0 + r, x1, y1 - r, color);     // right

    // Corner arcs sweep 90 degrees each.
    pushCorner(x0 + r, y0 + r, r, kPi, kPi * 1.5f, color, segments);       // top-left
    pushCorner(x1 - r, y0 + r, r, kPi * 1.5f, kPi * 2.0f, color, segments); // top-right
    pushCorner(x1 - r, y1 - r, r, 0.0f, kPi * 0.5f, color, segments);       // bottom-right
    pushCorner(x0 + r, y1 - r, r, kPi * 0.5f, kPi, color, segments);        // bottom-left
}

// Textures require GX2 surface upload and the texture shader; implemented in
// the next milestone.
Texture createTexture(int, int, const void *) { return -1; }
void destroyTexture(Texture) {}
void drawTexture(Texture, float, float, float, float) {}
void drawTextureCover(Texture, float, float, float, float) {}
void setScissor(int, int, int, int) {}
void clearScissor() {}

} // namespace retrou::gx2
