#include "gx2_renderer.hpp"

#include <whb/gfx.h>
#include <coreinit/cache.h>
#include <coreinit/memdefaultheap.h>
#include <gx2/clear.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/sampler.h>
#include <gx2/shaders.h>
#include <gx2/surface.h>
#include <gx2/swap.h>
#include <gx2/texture.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace retrou::gx2 {

namespace {

constexpr float kScreenW = 1280.0f;
constexpr float kScreenH = 720.0f;
constexpr float kPi = 3.14159265358979323846f;

#include "solid_gsh.inc"
#include "texture_gsh.inc"

WHBGfxShaderGroup g_solidGroup{};
WHBGfxShaderGroup g_textureGroup{};

std::vector<float> g_solidPos;
std::vector<float> g_solidCol;
std::vector<float> g_texPos;
std::vector<float> g_texUV;
std::vector<float> g_texCol;

struct DrawRun {
    bool textured;
    Texture texture;
    uint32_t first; // vertex offset within its buffer
    uint32_t count; // vertex count
};
std::vector<DrawRun> g_runs;
int g_runningKind = 0; // 0 none, 1 solid, 2 texture
Texture g_runningTexture = -1;

struct TextureRecord {
    GX2Texture texture{};
    int width = 0;
    int height = 0;
};
std::vector<TextureRecord *> g_textures;

GX2Sampler g_sampler{};
bool g_ready = false;
bool g_inFrame = false;

void *ensureMemory(void *&mem, size_t &capacity, size_t bytes)
{
    if (bytes > capacity) {
        if (mem) {
            MEMFreeToDefaultHeap(mem);
        }
        size_t newCapacity = std::max<size_t>(bytes, 4096);
        mem = MEMAllocFromDefaultHeapEx(newCapacity, 0x40);
        capacity = newCapacity;
    }
    return mem;
}

void flushCpu(void *mem, size_t bytes)
{
    DCFlushRange(mem, bytes);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU, mem, bytes);
}

TextureRecord *getTexture(Texture texture)
{
    int index = static_cast<int>(texture) - 1;
    if (index < 0 || index >= static_cast<int>(g_textures.size())) {
        return nullptr;
    }
    return g_textures[index];
}

void pushSolidVertex(float x, float y, Color c)
{
    if (g_runningKind != 1) {
        g_runs.push_back(DrawRun{false, -1,
                                 static_cast<uint32_t>(g_solidPos.size() / 2), 0});
        g_runningKind = 1;
    }
    const float ndcX = x / (kScreenW * 0.5f) - 1.0f;
    const float ndcY = 1.0f - y / (kScreenH * 0.5f);
    g_solidPos.push_back(ndcX);
    g_solidPos.push_back(ndcY);
    g_solidCol.push_back(c.r / 255.0f);
    g_solidCol.push_back(c.g / 255.0f);
    g_solidCol.push_back(c.b / 255.0f);
    g_solidCol.push_back(c.a / 255.0f);
    g_runs.back().count++;
}

void pushTexVertex(float x, float y, float u, float v, Color c, Texture texture)
{
    if (g_runningKind != 2 || g_runningTexture != texture) {
        g_runs.push_back(DrawRun{true, texture,
                                 static_cast<uint32_t>(g_texPos.size() / 2), 0});
        g_runningKind = 2;
        g_runningTexture = texture;
    }
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
    g_runs.back().count++;
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
        const float t0 = a0 + (a1 - a0) * i / segments;
        const float t1 = a0 + (a1 - a0) * (i + 1) / segments;
        pushTriangle(cx, cy,
                     cx + std::cos(t0) * radius, cy + std::sin(t0) * radius,
                     cx + std::cos(t1) * radius, cy + std::sin(t1) * radius, c);
    }
}

void flushFrame()
{
    if (g_runs.empty()) {
        return;
    }

    constexpr size_t posStride = 2 * sizeof(float);
    constexpr size_t colStride = 4 * sizeof(float);

    // Solid buffers.
    static void *solidPosMem = nullptr;
    static void *solidColMem = nullptr;
    static size_t solidPosCap = 0;
    static size_t solidColCap = 0;
    // Texture buffers.
    static void *texPosMem = nullptr;
    static void *texUvMem = nullptr;
    static void *texColMem = nullptr;
    static size_t texPosCap = 0;
    static size_t texUvCap = 0;
    static size_t texColCap = 0;

    if (!g_solidPos.empty()) {
        size_t posBytes = g_solidPos.size() * sizeof(float);
        size_t colBytes = g_solidCol.size() * sizeof(float);
        void *p = ensureMemory(solidPosMem, solidPosCap, posBytes);
        void *c = ensureMemory(solidColMem, solidColCap, colBytes);
        std::memcpy(p, g_solidPos.data(), posBytes);
        std::memcpy(c, g_solidCol.data(), colBytes);
        flushCpu(p, posBytes);
        flushCpu(c, colBytes);
    }
    if (!g_texPos.empty()) {
        size_t posBytes = g_texPos.size() * sizeof(float);
        size_t uvBytes = g_texUV.size() * sizeof(float);
        size_t colBytes = g_texCol.size() * sizeof(float);
        void *p = ensureMemory(texPosMem, texPosCap, posBytes);
        void *u = ensureMemory(texUvMem, texUvCap, uvBytes);
        void *c = ensureMemory(texColMem, texColCap, colBytes);
        std::memcpy(p, g_texPos.data(), posBytes);
        std::memcpy(u, g_texUV.data(), uvBytes);
        std::memcpy(c, g_texCol.data(), colBytes);
        flushCpu(p, posBytes);
        flushCpu(u, uvBytes);
        flushCpu(c, colBytes);
    }

    for (const DrawRun &run : g_runs) {
        if (run.count == 0) {
            continue;
        }
        if (!run.textured) {
            GX2SetFetchShader(&g_solidGroup.fetchShader);
            GX2SetVertexShader(g_solidGroup.vertexShader);
            GX2SetPixelShader(g_solidGroup.pixelShader);
            GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_REGISTER);
            GX2SetAttribBuffer(0, run.count * static_cast<uint32_t>(posStride),
                               posStride,
                               static_cast<uint8_t *>(solidPosMem) +
                                   run.first * posStride);
            GX2SetAttribBuffer(1, run.count * static_cast<uint32_t>(colStride),
                               colStride,
                               static_cast<uint8_t *>(solidColMem) +
                                   run.first * colStride);
            GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, run.count, 0, 1);
        } else {
            TextureRecord *record = getTexture(run.texture);
            if (!record) {
                continue;
            }
            GX2SetFetchShader(&g_textureGroup.fetchShader);
            GX2SetVertexShader(g_textureGroup.vertexShader);
            GX2SetPixelShader(g_textureGroup.pixelShader);
            GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_REGISTER);
            GX2SetAttribBuffer(0, run.count * static_cast<uint32_t>(posStride),
                               posStride,
                               static_cast<uint8_t *>(texPosMem) +
                                   run.first * posStride);
            GX2SetAttribBuffer(1, run.count * static_cast<uint32_t>(posStride),
                               posStride,
                               static_cast<uint8_t *>(texUvMem) +
                                   run.first * posStride);
            GX2SetAttribBuffer(2, run.count * static_cast<uint32_t>(colStride),
                               colStride,
                               static_cast<uint8_t *>(texColMem) +
                                   run.first * colStride);
            const uint32_t unit =
                g_textureGroup.pixelShader->samplerVars[0].location;
            GX2SetPixelTexture(&record->texture, unit);
            GX2SetPixelSampler(&g_sampler, unit);
            GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, run.count, 0, 1);
        }
    }

    g_runs.clear();
    g_solidPos.clear();
    g_solidCol.clear();
    g_texPos.clear();
    g_texUV.clear();
    g_texCol.clear();
    g_runningKind = 0;
    g_runningTexture = -1;
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
    WHBGfxInitShaderAttribute(&g_solidGroup, "aPos", 0, 0,
                              GX2_ATTRIB_FORMAT_FLOAT_32_32);
    WHBGfxInitShaderAttribute(&g_solidGroup, "aColor", 1, 0,
                              GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
    WHBGfxInitFetchShader(&g_solidGroup);
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
        if (record && record->texture.surface.image) {
            MEMFreeToDefaultHeap(record->texture.surface.image);
        }
        delete record;
    }
    g_textures.clear();
    WHBGfxFreeShaderGroup(&g_solidGroup);
    WHBGfxFreeShaderGroup(&g_textureGroup);
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
    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, FALSE, TRUE);
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
    GX2SetBlendControl(GX2_RENDER_TARGET_0,
                       GX2_BLEND_MODE_SRC_ALPHA,
                       GX2_BLEND_MODE_INV_SRC_ALPHA,
                       GX2_BLEND_COMBINE_MODE_ADD,
                       TRUE,
                       GX2_BLEND_MODE_ONE,
                       GX2_BLEND_MODE_ZERO,
                       GX2_BLEND_COMBINE_MODE_ADD);
    g_inFrame = true;
}

void endFrame()
{
    if (!g_ready) {
        return;
    }
    flushFrame();
    WHBGfxFinishRenderTV();
    GX2CopyColorBufferToScanBuffer(WHBGfxGetTVColourBuffer(), GX2_SCAN_TARGET_DRC);
    WHBGfxFinishRender();
    g_inFrame = false;
}

void drawRect(float x, float y, float w, float h, Color color)
{
    if (!g_ready || !g_inFrame) return;
    pushQuad(x, y, x + w, y + h, color);
}

void drawGradient(float x, float y, float w, float h, Color top, Color bottom)
{
    if (!g_ready || !g_inFrame) return;
    pushGradientQuad(x, y, x + w, y + h, top, bottom);
}

void drawRoundRect(float x, float y, float w, float h, float radius, Color color)
{
    if (!g_ready || !g_inFrame) return;
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
    const uint8_t *src = static_cast<const uint8_t *>(rgba);
    for (int row = 0; row < height; ++row) {
        uint32_t *dst = static_cast<uint32_t *>(surface.image) + row * surface.pitch;
        std::memcpy(dst, src + static_cast<size_t>(row) * width * 4,
                    static_cast<size_t>(width) * 4);
    }
    DCFlushRange(surface.image, surface.imageSize);
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

void updateTexture(Texture texture, const void *rgba)
{
    TextureRecord *record = getTexture(texture);
    if (!record || !rgba) {
        return;
    }
    GX2Surface &surface = record->texture.surface;
    const uint8_t *src = static_cast<const uint8_t *>(rgba);
    for (int row = 0; row < record->height; ++row) {
        uint32_t *dst = static_cast<uint32_t *>(surface.image) + row * surface.pitch;
        std::memcpy(dst, src + static_cast<size_t>(row) * record->width * 4,
                    static_cast<size_t>(record->width) * 4);
    }
    DCFlushRange(surface.image, surface.imageSize);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_TEXTURE,
                  surface.image, surface.imageSize);
}

Texture uploadFrame(Texture existing, const void *data, int width, int height,
                    int pitch, bool rgb565)
{
    if (width <= 0 || height <= 0 || !data) {
        return -1;
    }
    std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4);
    const uint8_t *src = static_cast<const uint8_t *>(data);
    for (int y = 0; y < height; ++y) {
        const uint8_t *row = src + static_cast<size_t>(y) * pitch;
        for (int x = 0; x < width; ++x) {
            uint8_t *dst = &rgba[(static_cast<size_t>(y) * width + x) * 4];
            if (rgb565) {
                const uint16_t px = reinterpret_cast<const uint16_t *>(row)[x];
                uint8_t r = static_cast<uint8_t>((px >> 11) & 0x1F);
                uint8_t g = static_cast<uint8_t>((px >> 5) & 0x3F);
                uint8_t b = static_cast<uint8_t>(px & 0x1F);
                dst[0] = static_cast<uint8_t>((r << 3) | (r >> 2));
                dst[1] = static_cast<uint8_t>((g << 2) | (g >> 4));
                dst[2] = static_cast<uint8_t>((b << 3) | (b >> 2));
                dst[3] = 255;
            } else {
                // XRGB8888: assume bytes are R,G,B,X (the common libretro layout
                // for the static cores). Verified against hardware if needed.
                dst[0] = row[x * 4 + 0];
                dst[1] = row[x * 4 + 1];
                dst[2] = row[x * 4 + 2];
                dst[3] = 255;
            }
        }
    }
    if (existing >= 0 && getTexture(existing)) {
        updateTexture(existing, rgba.data());
        return existing;
    }
    return createTexture(width, height, rgba.data());
}

void drawTexturedRect(Texture texture, float x, float y, float w, float h,
                      float u0, float v0, float u1, float v1, Color tint)
{
    if (!g_ready || !g_inFrame || !getTexture(texture)) {
        return;
    }
    pushTexVertex(x, y, u0, v0, tint, texture);
    pushTexVertex(x + w, y, u1, v0, tint, texture);
    pushTexVertex(x + w, y + h, u1, v1, tint, texture);
    pushTexVertex(x, y, u0, v0, tint, texture);
    pushTexVertex(x + w, y + h, u1, v1, tint, texture);
    pushTexVertex(x, y + h, u0, v1, tint, texture);
}

void drawTexture(Texture texture, float x, float y, float w, float h)
{
    if (!g_ready || !g_inFrame) return;
    if (!getTexture(texture)) return;
    const Color white{255, 255, 255, 255};
    pushTexVertex(x, y, 0.0f, 0.0f, white, texture);
    pushTexVertex(x + w, y, 1.0f, 0.0f, white, texture);
    pushTexVertex(x + w, y + h, 1.0f, 1.0f, white, texture);
    pushTexVertex(x, y, 0.0f, 0.0f, white, texture);
    pushTexVertex(x + w, y + h, 1.0f, 1.0f, white, texture);
    pushTexVertex(x, y + h, 0.0f, 1.0f, white, texture);
}

void drawTextureTinted(Texture texture, float x, float y, float w, float h, Color tint)
{
    if (!g_ready || !g_inFrame) return;
    if (!getTexture(texture)) return;
    pushTexVertex(x, y, 0.0f, 0.0f, tint, texture);
    pushTexVertex(x + w, y, 1.0f, 0.0f, tint, texture);
    pushTexVertex(x + w, y + h, 1.0f, 1.0f, tint, texture);
    pushTexVertex(x, y, 0.0f, 0.0f, tint, texture);
    pushTexVertex(x + w, y + h, 1.0f, 1.0f, tint, texture);
    pushTexVertex(x, y + h, 0.0f, 1.0f, tint, texture);
}

void drawTextureCover(Texture texture, float x, float y, float w, float h)
{
    if (!g_ready || !g_inFrame) return;
    TextureRecord *record = getTexture(texture);
    if (!record) return;
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
    pushTexVertex(x, y, u0, v0, white, texture);
    pushTexVertex(x + w, y, u1, v0, white, texture);
    pushTexVertex(x + w, y + h, u1, v1, white, texture);
    pushTexVertex(x, y, u0, v0, white, texture);
    pushTexVertex(x + w, y + h, u1, v1, white, texture);
    pushTexVertex(x, y + h, u0, v1, white, texture);
}

void setScissor(int, int, int, int) {}
void clearScissor() {}

} // namespace retrou::gx2
