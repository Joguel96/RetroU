#include "gx2_renderer.hpp"

#include <whb/gfx.h>
#include <gx2/clear.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/shaders.h>
#include <gx2/swap.h>
#include <gx2r/buffer.h>
#include <gx2/texture.h>
#include <gx2/sampler.h>

namespace retrou::gx2 {

namespace {

// Embedded shader binaries. These are produced from the .vert/.frag files
// under shaders/ via CafeGLSL's glslcompiler and embedded by the build. They
// are declared here so the renderer links against the generated objects.
extern const unsigned char _binary_solid_gsh_start[];
extern const unsigned char _binary_solid_gsh_end[];
extern const unsigned char _binary_texture_gsh_start[];
extern const unsigned char _binary_texture_gsh_end[];

WHBGfxShaderGroup g_solidGroup{};
WHBGfxShaderGroup g_textureGroup{};
bool g_ready = false;
bool g_inFrame = false;

} // namespace

bool init()
{
    if (g_ready) {
        return true;
    }

    if (!WHBGfxInit()) {
        return false;
    }

    // Load the two shader pairs (solid color + textured) from embedded GFD data.
    const size_t solidSize =
        static_cast<size_t>(_binary_solid_gsh_end - _binary_solid_gsh_start);
    const size_t textureSize =
        static_cast<size_t>(_binary_texture_gsh_end - _binary_texture_gsh_start);
    if (solidSize > 0 &&
        !WHBGfxLoadGFDShaderGroup(&g_solidGroup, 0, _binary_solid_gsh_start)) {
        WHBGfxShutdown();
        return false;
    }
    if (textureSize > 0 &&
        !WHBGfxLoadGFDShaderGroup(&g_textureGroup, 0, _binary_texture_gsh_start)) {
        WHBGfxShutdown();
        return false;
    }

    // Attribute layout matches the GLSL:
    //   solid:   location 0 = aPos (vec2), location 1 = aColor (vec4)
    //   texture: location 0 = aPos (vec2), location 1 = aUV (vec2),
    //            location 2 = aColor (vec4)
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
    // Single render pass: copy the one TV color buffer to the TV scan buffer,
    // then to the DRC (GamePad) scan buffer so both displays show the same
    // frame. No separate render target is drawn for the GamePad.
    WHBGfxFinishRenderTV();
    GX2CopyColorBufferToScanBuffer(WHBGfxGetTVColourBuffer(), GX2_SCAN_TARGET_DRC);
    WHBGfxFinishRender();
    g_inFrame = false;
}

// Drawing helpers are wired in the next milestone. They will batch quads into
// a vertex buffer and issue GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, ...).
void drawRect(float, float, float, float, Color) { (void)g_inFrame; }
void drawRoundRect(float, float, float, float, float, Color) { (void)g_inFrame; }
void drawGradient(float, float, float, float, Color, Color) { (void)g_inFrame; }

Texture createTexture(int, int, const void *) { return -1; }
void destroyTexture(Texture) {}
void drawTexture(Texture, float, float, float, float) {}
void drawTextureCover(Texture, float, float, float, float) {}
void setScissor(int, int, int, int) {}
void clearScissor() {}

} // namespace retrou::gx2
