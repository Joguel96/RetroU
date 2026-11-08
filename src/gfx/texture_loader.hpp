#pragma once

// =============================================================================
// texture_loader.hpp - Caratulas PNG/JPG -> GX2Texture en VRAM
// =============================================================================
//
// Carga imagenes desde la SD:
//   fs:/vol/external01/retroU/covers/[sistema]/[nombre].png (o .jpg)
//
// Flujo (dos pasos para poder probar cada uno por separado):
//   1. decodeFile(): lectura via coreinit FS + stb_image -> RGBA8 en RAM.
//      (stbi_load_from_memory: NO depende de stdio sobre FS)
//   2. createTexture(): RGBA8 -> GX2Texture con memoria GX2R en VRAM/MEM2
//      (GX2RCreateSurface con BIND_TEXTURE | CPU_WRITE | GPU_READ,
//      tile LINEAR_ALIGNED para subida directa desde CPU).
//
// REQUISITO de createTexture(): GX2 inicializado (GX2Init/WHBGfxInit del
// backend GX2, fase 2 de render) porque GX2InitTextureRegs toca registros
// de la GPU. El backend OSScreen de fase 1 NO inicializa GX2 para dibujo:
// por eso el grid muestra placeholders hasta que exista dicho backend.
// (Verificado: gx2/texture.h solo expone GX2InitTextureRegs + setters;
// la creacion real es via gx2r/surface.h - GX2RCreateSurface/Lock/Unlock.)

#include <cstdint>
#include <string>
#include <vector>

#include <gx2/texture.h>

namespace retroU {

// Imagen RGBA8 lineal (fila-major, origen arriba-izquierda, 4 bytes/px).
struct DecodedImage {
    std::vector<uint8_t> pixels;
    uint32_t width  = 0;
    uint32_t height = 0;

    bool valid() const { return !pixels.empty() && width > 0 && height > 0; }
};

class TextureLoader {
public:
    // Lee el archivo de la SD y lo decodifica a RGBA8 (PNG o JPG, segun
    // decida stb_image por magic bytes). false = no existe / corrupto /
    // demasiorado (> maxBytes).
    static bool decodeFile(const std::string &path, DecodedImage &out,
                           uint32_t maxBytes = 8u * 1024u * 1024u);

    // Crea la GX2Texture en memoria GX2R (VRAM/MEM2) y sube los pixels.
    // 'out' se sobreescribe. Devuelve false si GX2R rechazo la creacion.
    // REQUISITO: GX2 ya inicializado (fase 2 - backend GX2).
    static bool createTexture(const DecodedImage &image, GX2Texture &out);

    // Libera la memoria GX2R asociada a la textura (idempotente).
    static void destroyTexture(GX2Texture &texture);

    // Ruta de caratula: coversBaseDir() + "/[sistema]/[romBase].png|.jpg".
    // 'romBase' es RomInfo::title (nombre del archivo sin extension).
    static std::string coverPath(const char *systemFolder,
                                 const char *romBase, bool png);
};

} // namespace retroU
