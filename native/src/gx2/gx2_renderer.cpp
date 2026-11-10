#include "gx2_renderer.hpp"

#include <whb/gfx.h>
#include <coreinit/memdefaultheap.h>
#include <gx2/clear.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/sampler.h>
#include <gx2/shaders.h>
#include <gx2/surface.h>
#include <gx2/swap.h>
#include <gx2/texture.h>
#include <gx2r/buffer.h>
#include <gx2r/draw.h>
#include <gx2r/surface.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace retrou::gx2 {

namespace {

// Logical resolution of the single render target (16:9).
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

GX2RBuffer g_texPosBuffer{};
GX2RBuffer g_texUVBuffer{};
GX2RBuffer g_texColBuffer{};
std::vector<float> g_texPos; // 2 floats per vertex
std::vector<float> g_texUV;  // 2 floats per vertex
std::vector<float> g_texCol; // 4 floats per vertex

struct TextureRecord {
    GX2Texture texture{};
    int width = 0;
    int height = 0;
};
std::vector<TextureRecord *> g_textures;

GX2Sampler g_sampler{};

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

void pushTexVertex(float x, float y, float u, float v, Color c)
{
    const float ndcX = x / (kScreenW * 0.5f) - 1.0f;
    const float ndcY = 1.0f - y / (kScreenH * 0.5f);
    g_texPos.push_back(ndcX);
    g_texPos.push_back(ndcY);
    g_texUV.push_back(u);
    g_texUV.push_back(v);
    g_texCol.push_back(c.r / 255.0f);
    g_texCol.push_back(c.g / 255.0f);
    g_texCol.push_back(c.b / 255.0f);
    g_texCol.push_back(c.a / 255.0f);
}

void flushTexture(TextureRecord *record)
{
    if (!record || g_texPos.empty()) {
        return;
    }

    uploadBuffer(g_texPosBuffer, g_texPos);
    uploadBuffer(g_texUVBuffer, g_texUV);
    uploadBuffer(g_texColBuffer, g_texCol);

    GX2SetFetchShader(&g_textureGroup.fetchShader);
    GX2SetVertexShader(g_textureGroup.vertexShader);
    GX2SetPixelShader(g_textureGroup.pixelShader);
    GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_REGISTER);

    GX2RSetAttributeBuffer(&g_texPosBuffer, 0, 2 * sizeof(float), 0);
    GX2RSetAttributeBuffer(&g_texUVBuffer, 1, 2 * sizeof(float), 0);
    GX2RSetAttributeBuffer(&g_texColBuffer, 2, 4 * sizeof(float), 0);

    const uint32_t unit = g_textureGroup.pixelShader->samplerVars[0].location;
    GX2SetPixelTexture(&record->texture, unit);
    GX2SetPixelSampler(&g_sampler, unit);

    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES,
              static_cast<uint32_t>(g_texPos.size() / 2), 0, 1);

    g_texPos.clear();
    g_texUV.clear();
    g_texCol.clear();
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

    GX2InitSampler(&g_sampler, GX2_TEX_CLAMP_MODE_CLAMP,
                   GX2_TEX_XY_FILTER_MODE_LINEAR);

    g_ready = true;
    return true;
}

void shutdown()
{
    if (!g_ready) {
        return;
    }
    for (TextureRecord *record : g_textures) {
        GX2RDestroySurfaceEx(&record->texture.surface, GX2R_RESOURCE_BIND_NONE);
        delete record;
    }
    g_textures.clear();
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

    pushQuad(x0 + r, y0 + r, x1 - r, y1 - r, color);
    pushQuad(x0 + r, y0, x1 - r, y0 + r, color);
    pushQuad(x0 + r, y1 - r, x1 - r, y1, color);
    pushQuad(x0, y0 + r, x0 + r, y1 - r, color);
    pushQuad(x1 - r, y0 + r, x1, y1 - r, color);

    pushCorner(x0 + r, y0 + r, r, kPi, kPi * 1.5f, color, segments);
    pushCorner(x1 - r, y0 + r, r, kPi * 1.5f, kPi * 2.0f, color, segments);
    pushCorner(x1 - r, y1 - r, r, 0.0f, kPi * 0.5f, color, segments);
    pushCorner(x0 + r, y1 - r, r, kPi * 0.5f, kPi, color, segments);
}

Texture createTexture(int width, int height, const void *rgba)
{
    if (!g_ready || width <= 0 || height <= 0 || !rgba) {
        return -1;
    }

    TextureRecord *record = new TextureRecord();
    GX2Texture &tex = record->texture;
    GX2Surface &surface = tex.surface;
    std::memset(&tex, 0, sizeof(tex));

    surface.width = static_cast<uint32_t>(width);
    surface.height = static_cast<uint32_t>(height);
    surface.depth = 1;
    surface.mipLevels = 1;
    surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    surface.aa = GX2_AA_MODE1X;
    surface.use = GX2_SURFACE_USE_TEXTURE;
    surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    surface.swizzle = 0;
    tex.viewFirstMip = 0;
    tex.viewNumMips = 1;
    tex.viewFirstSlice = 0;
    tex.viewNumSlices = 1;
    tex.compMap = 0x0010203;

    GX2CalcSurfaceSizeAndAlignment(&surface);
    GX2InitTextureRegs(&tex);
    if (surface.imageSize == 0) {
        delete record;
        return -1;
    }

    surface.image = MEMAllocFromDefaultHeapEx(surface.imageSize, surface.alignment);
    if (!surface.image) {
        delete record;
        return -1;
    }

    // For LINEAR_ALIGNED surfaces with a 32-bit format, GX2Surface::pitch is
    // expressed in uint32_t elements, so the byte stride is pitch * 4. This
    // matches the known-good CafeGLSL texture upload path.
    const uint8_t *src = static_cast<const uint8_t *>(rgba);
    for (int row = 0; row < height; ++row) {
        uint32_t *dst = static_cast<uint32_t *>(surface.image) +
                        row * surface.pitch;
        std::memcpy(dst, src + static_cast<size_t>(row) * width * 4,
                    static_cast<size_t>(width) * 4);
    }

    GX2Invalidate(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_TEXTURE,
                  surface.image, surface.imageSize);

    record->width = width;
    record->height = height;
    g_textures.push_back(record);
    return static_cast<Texture>(g_textures.size());
}

void destroyTexture(Texture texture)
{
    int index = static_cast<int>(texture) - 1;
    if (index < 0 || index >= static_cast<int>(g_textures.size()) || !g_textures[index]) {
        return;
    }
    TextureRecord *record = g_textures[index];
    if (record->texture.surface.image) {
        MEMFreeToDefaultHeap(record->texture.surface.image);
    }
    delete record;
    g_textures[index] = nullptr;
}

TextureRecord *getTexture(Texture texture)
{
    int index = static_cast<int>(texture) - 1;
    if (index < 0 || index >= static_cast<int>(g_textures.size())) {
        return nullptr;
    }
    return g_textures[index];
}

void drawTexture(Texture texture, float x, float y, float w, float h)
{
    if (!g_ready || !g_inFrame) {
        return;
    }
    TextureRecord *record = getTexture(texture);
    if (!record) {
        return;
    }

    flushSolid();

    const Color white{255, 255, 255, 255};
    pushTexVertex(x, y, 0.0f, 0.0f, white);
    pushTexVertex(x + w, y, 1.0f, 0.0f, white);
    pushTexVertex(x + w, y + h, 1.0f, 1.0f, white);
    pushTexVertex(x, y, 0.0f, 0.0f, white);
    pushTexVertex(x + w, y + h, 1.0f, 1.0f, white);
    pushTexVertex(x, y + h, 0.0f, 1.0f, white);

    flushTexture(record);
}

void drawTextureTinted(Texture texture, float x, float y, float w, float h, Color tint)
{
    if (!g_ready || !g_inFrame) {
        return;
    }
    TextureRecord *record = getTexture(texture);
    if (!record) {
        return;
    }

    flushSolid();

    pushTexVertex(x, y, 0.0f, 0.0f, tint);
    pushTexVertex(x + w, y, 1.0f, 0.0f, tint);
    pushTexVertex(x + w, y + h, 1.0f, 1.0f, tint);
    pushTexVertex(x, y, 0.0f, 0.0f, tint);
    pushTexVertex(x + w, y + h, 1.0f, 1.0f, tint);
    pushTexVertex(x, y + h, 0.0f, 1.0f, tint);

    flushTexture(record);
}

void drawTextureCover(Texture texture, float x, float y, float w, float h)
{
    if (!g_ready || !g_inFrame) {
        return;
    }
    TextureRecord *record = getTexture(texture);
    if (!record) {
        return;
    }

    flushSolid();

    float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
    const float srcAspect = record->height > 0
        ? static_cast<float>(record->width) / record->height : 1.0f;
    const float dstAspect = h > 0.0f ? w / h : 1.0f;
    if (srcAspect > dstAspect) {
        const float scale = dstAspect / srcAspect;
        u0 = (1.0f - scale) * 0.5f;
        u1 = 1.0f - u0;
    } else {
        const float scale = srcAspect / dstAspect;
        v0 = (1.0f - scale) * 0.5f;
        v1 = 1.0f - v0;
    }

    const Color white{255, 255, 255, 255};
    pushTexVertex(x, y, u0, v0, white);
    pushTexVertex(x + w, y, u1, v0, white);
    pushTexVertex(x + w, y + h, u1, v1, white);
    pushTexVertex(x, y, u0, v0, white);
    pushTexVertex(x + w, y + h, u1, v1, white);
    pushTexVertex(x, y + h, u0, v1, white);

    flushTexture(record);
}

void setScissor(int, int, int, int) {}
void clearScissor() {}

} // namespace retrou::gx2
