// =============================================================================
// texture_loader.cpp - Decodificacion PNG/JPG + creacion GX2Texture (GX2R)
// =============================================================================
//
// APIs verificadas en los headers instalados (WUT 1.9.2):
//   fs (coreinit/filesystem.h): FSOpenFile/FSReadFile/FSCloseFile/FSGetStat
//   stb_image (third_party):    stbi_load_from_memory -> RGBA8 forcado
//   gx2/surface.h:              GX2CalcSurfaceSizeAndAlignment(GX2Surface*)
//   gx2r/surface.h:             GX2RCreateSurface(surface, flags),
//                               GX2RLockSurfaceEx(surface, level, flags),
//                               GX2RUnlockSurfaceEx(...),
//                               GX2RDestroySurfaceEx(...)
//   gx2/texture.h:              struct GX2Texture { surface; view...;
//                               compMap; regs[5]; } + GX2InitTextureRegs()
//   gx2/enum.h:                 GX2_SURFACE_DIM_TEXTURE_2D,
//                               GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8 (0x01a),
//                               GX2_AA_MODE1X (0), GX2_TILE_MODE_LINEAR_ALIGNED
//                               (1), GX2_INVALIDATE_MODE_TEXTURE
//   gx2r/resource.h:            GX2R_RESOURCE_BIND_TEXTURE (1<<0),
//                               USAGE_CPU_WRITE (1<<12), USAGE_GPU_READ (1<<13)
//
// LINEAR_ALIGNED (no tiled): la subida desde CPU es un memcpy por fila sin
// swizzle; para caratulas de UI (pocos draws, una por frame) el bandwidth
// lineal es mas que suficiente y evita GX2Swizzle*.

#include "gfx/texture_loader.hpp"
#include "fs/fs.hpp"

#include <gx2/enum.h>
#include <gx2/mem.h>       // GX2Invalidate
#include <gx2/surface.h>   // GX2CalcSurfaceSizeAndAlignment
#include <gx2r/resource.h> // flags GX2R
#include <gx2r/surface.h>  // GX2RCreate/Lock/Unlock/Destroy

#include <cstring>

// stb_image: implementacion unica en third_party/stb/stb_image_impl.cpp
#include "stb_image.h"

namespace retroU {

bool TextureLoader::decodeFile(const std::string &path, DecodedImage &out,
                               uint32_t maxBytes)
{
    out = DecodedImage{};

    // 1) Leer el archivo crudo de la SD via coreinit FS (sin stdio: el
    //    soporte de fopen sobre fs:/ no es portable en todos los setups).
    std::vector<uint8_t> raw;
    if (!fs::readFile(path, raw, maxBytes) || raw.empty()) {
        return false;
    }

    // 2) Decodificar a RGBA8 (canal alpha forzado aunque el PNG sea RGB).
    int w = 0, h = 0, comp = 0;
    stbi_uc *data = stbi_load_from_memory(raw.data(), static_cast<int>(raw.size()),
                                          &w, &h, &comp, 4 /* RGBA */);
    if (!data || w <= 0 || h <= 0) {
        if (data) {
            stbi_image_free(data);
        }
        return false;
    }

    out.width  = static_cast<uint32_t>(w);
    out.height = static_cast<uint32_t>(h);
    out.pixels.assign(data, data + static_cast<size_t>(w) * h * 4);
    stbi_image_free(data);
    return out.valid();
}

bool TextureLoader::createTexture(const DecodedImage &image, GX2Texture &out)
{
    if (!image.valid()) {
        return false;
    }

    // Limpia la struct completa (0x9c bytes, WUT_CHECK_SIZE): los campos
    // view*/compMap/regs no se tocan si no se piden.
    std::memset(&out, 0, sizeof(out));

    GX2Surface &s = out.surface;
    s.dim        = GX2_SURFACE_DIM_TEXTURE_2D;
    s.width      = image.width;
    s.height     = image.height;
    s.depth      = 1;
    s.mipLevels  = 1; // sin mipmaps para covers
    s.format     = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    s.aa         = GX2_AA_MODE1X;
    s.resourceFlags = GX2R_RESOURCE_BIND_TEXTURE |
                      GX2R_RESOURCE_USAGE_CPU_WRITE |
                      GX2R_RESOURCE_USAGE_GPU_READ;
    s.tileMode   = GX2_TILE_MODE_LINEAR_ALIGNED;
    s.swizzle    = 0;

    // Calcula imageSize/alignment/pitch segun formato y tileMode.
    GX2CalcSurfaceSizeAndAlignment(&s);

    if (!GX2RCreateSurface(&s, s.resourceFlags)) {
        return false; // sin memoria GX2R (MEM2) para esta caratula
    }

    // Subida CPU -> memoria de la textura (level 0).
    void *dst = GX2RLockSurfaceEx(&s, 0, GX2R_RESOURCE_USAGE_CPU_WRITE);
    if (!dst) {
        GX2RDestroySurfaceEx(&s, s.resourceFlags);
        std::memset(&out, 0, sizeof(out));
        return false;
    }

    const uint32_t rowBytes = image.width * 4u;
    const uint32_t pitch    = (s.pitch != 0) ? s.pitch : rowBytes;
    const uint8_t *src      = image.pixels.data();
    uint8_t *base           = static_cast<uint8_t *>(dst);

    if (pitch == rowBytes) {
        // Caso comun: filas contiguas -> una sola copia.
        std::memcpy(base, src, static_cast<size_t>(rowBytes) * image.height);
    } else {
        for (uint32_t y = 0; y < image.height; ++y) {
            std::memcpy(base + static_cast<size_t>(y) * pitch,
                        src + static_cast<size_t>(y) * rowBytes, rowBytes);
        }
    }

    GX2RUnlockSurfaceEx(&s, 0, GX2R_RESOURCE_USAGE_CPU_WRITE);

    // View + compMap estandar RGBA sin remapear (0x00112233) y registros
    // de textura de la GPU. REQUIERE GX2 inicializado (fase 2).
    out.viewFirstMip  = 0;
    out.viewNumMips   = 1;
    out.viewFirstSlice = 0;
    out.viewNumSlices = 1;
    out.compMap       = 0x00112233;
    GX2InitTextureRegs(&out);

    // CPU -> memoria fisica coherente antes de que la GPU la muestree.
    GX2Invalidate(GX2_INVALIDATE_MODE_TEXTURE, s.image, s.imageSize);
    return true;
}

void TextureLoader::destroyTexture(GX2Texture &texture)
{
    if (texture.surface.image != nullptr ||
        (texture.surface.resourceFlags & GX2R_RESOURCE_GX2R_ALLOCATED)) {
        GX2RDestroySurfaceEx(&texture.surface, texture.surface.resourceFlags);
    }
    std::memset(&texture, 0, sizeof(texture));
}

std::string TextureLoader::coverPath(const char *systemFolder,
                                     const char *romBase, bool png)
{
    std::string p = fs::coversBaseDir();
    p += "/";
    p += systemFolder;
    p += "/";
    p += romBase;
    p += png ? ".png" : ".jpg";
    return p;
}

} // namespace retroU
