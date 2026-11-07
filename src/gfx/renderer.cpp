// =============================================================================
// renderer.cpp - Implementacion del backend OSScreen (fase 1 de RetroU)
// =============================================================================
//
// TODAS las llamadas usadas aqui estan verificadas contra:
//   - WUT include/coreinit/{screen,memory,cache,time}.h (descargados y
//     revisados firma por firma durante el desarrollo; NO inventados)
//   - WiiUBrew "Coreinit.rpl" seccion Screen (semantica de hardware real:
//     buffers primario+secundario, alineacion 0x100, flips que alternan)
//   - decaf (emulador Wii U) coreinit_screen.cpp (mismas firmas/formatos)
//   - libwhb (WUT) console.c  y  libwiiu draw.c (patrones canonicos)
//
// Contratodor por pantalla de doble buffer interno: OSScreenGetBufferSizeEx
// devuelve el tamanho TOTAL (primario + secundario). El work buffer sobre
// el que OSScreen* escribe NO es un puntero publico: por eso las primitivas
// nativas (Clear/PutPixel/PutFont) son la via segura, y por eso arriba
// WHB hace DCFlushRange + OSScreenFlipBuffersEx sin tocar punteros crudos.

#include "gfx/renderer.hpp"

#include <coreinit/screen.h>   // OSScreen* (SCREEN_TV/SCREEN_DRC, funcs)
#include <coreinit/memory.h>   // OSAllocFromSystem / OSFreeToSystem
#include <coreinit/cache.h>    // DCFlushRange (cache -> memoria antes del flip)

namespace retroU {

bool Renderer::init()
{
    // 1) Inicializa el subsistema de pantalla de coreinit (activa registros
    //    de framebuffer internos de GX2 desde el SDK real).
    OSScreenInit();

    // 2) Reserva memoria para los DOS framebuffers (primario+secundario)
    //    de cada pantalla. Debe venir del area del sistema, alineada 0x100.
    m_tvSize  = OSScreenGetBufferSizeEx(SCREEN_TV);   //  2 * 0x384000
    m_drcSize = OSScreenGetBufferSizeEx(SCREEN_DRC);  //  2 * 0x1A3800 aprox

    m_tvFramebuffer  = OSAllocFromSystem(m_tvSize, 0x100);
    m_drcFramebuffer = OSAllocFromSystem(m_drcSize, 0x100);

    if (!m_tvFramebuffer || !m_drcFramebuffer) {
        shutdown();
        return false;
    }

    // 3) Registra las areas. A partir de aqui Y DESPUES SE HABILITA el
    //    display. Sin buffers registrados, cualquier draw crashea la app
    //    (ver WiiUBrew: "These functions can not be used without crashing,
    //    unless OSScreen was properly initialized").
    OSScreenSetBufferEx(SCREEN_TV,  m_tvFramebuffer);
    OSScreenSetBufferEx(SCREEN_DRC, m_drcFramebuffer);

    // 4) Habilita la salida de ambos displays (negro mientras no flippeamos).
    OSScreenEnableEx(SCREEN_TV,  1);
    OSScreenEnableEx(SCREEN_DRC, 1);

    return true;
}

void Renderer::shutdown()
{
    // Orden inverso de init; seguro de llamar aunque init fallo (idempotente).
    if (m_tvFramebuffer)  { OSFreeToSystem(m_tvFramebuffer);  m_tvFramebuffer  = nullptr; }
    if (m_drcFramebuffer) { OSFreeToSystem(m_drcFramebuffer); m_drcFramebuffer = nullptr; }
    m_tvSize = m_drcSize = 0;

    // OSScreenShutdown() del SDK real solo hace EnableEx(0/1, false).
    OSScreenShutdown();
}

void Renderer::clear(uint32_t color)
{
    // Rellena el WORK buffer completo de cada pantalla (u32 x u32).
    // No aparece en pantalla hasta el siguiente flip (present()).
    OSScreenClearBufferEx(SCREEN_TV,  color);
    OSScreenClearBufferEx(SCREEN_DRC, color);
}

void Renderer::drawRect(int x, int y, int w, int h, uint32_t color)
{
    if (w <= 0 || h <= 0) return;

    // Coordenadas virtuales: el sistema "logico" del UI es 1280x720 (TV).
    // El mismo rect se dibuja en cada display en su resolucion nativa,
    // escalando una vez por display (no pixel-a-pixel cruzado).
    const int x1 = (x < 0) ? 0 : x;
    const int y1 = (y < 0) ? 0 : y;
    const int x2 = x + w;
    const int y2 = y + h;

    // Clipping + escritura en el TV (1280x720, coords 1:1)
    for (int py = y1; py < y2; ++py) {
        if (py < 0 || py >= static_cast<int>(TV_HEIGHT)) continue;
        for (int px = x1; px < x2; ++px) {
            if (px < 0 || px >= static_cast<int>(TV_WIDTH)) continue;
            OSScreenPutPixelEx(SCREEN_TV, px, py, color);
        }
    }

    // Mismo rect escalado al gamepad (854x480)
    const int dx1 = x1 * static_cast<int>(DRC_WIDTH)  / static_cast<int>(TV_WIDTH);
    const int dy1 = y1 * static_cast<int>(DRC_HEIGHT) / static_cast<int>(TV_HEIGHT);
    const int dx2 = x2 * static_cast<int>(DRC_WIDTH)  / static_cast<int>(TV_WIDTH);
    const int dy2 = y2 * static_cast<int>(DRC_HEIGHT) / static_cast<int>(TV_HEIGHT);

    for (int py = dy1; py < dy2; ++py) {
        if (py < 0 || py >= static_cast<int>(DRC_HEIGHT)) continue;
        for (int px = dx1; px < dx2; ++px) {
            if (px < 0 || px >= static_cast<int>(DRC_WIDTH)) continue;
            OSScreenPutPixelEx(SCREEN_DRC, px, py, color);
        }
    }
}

void Renderer::drawText(uint32_t column, uint32_t row, const char *text)
{
    if (!text) return;
    OSScreenPutFontEx(SCREEN_TV,  column, row, text);
    // En el gamepad usamos la misma cuadricula: la fuente interna escala
    // igual (posiciones en celdas de caracter) en ambos displays.
    OSScreenPutFontEx(SCREEN_DRC, column, row, text);
}

void Renderer::present()
{
    // Flush del cache de datos de las (grandes) areas de framebuffer a
    // memoria fisica ANTES del flip: practica canonica de libwhb/console.c.
    // Sin esto, los ultimos writes de drawRect pueden no llegar a GDDR.
    if (m_tvFramebuffer)  DCFlushRange(m_tvFramebuffer,  m_tvSize);
    if (m_drcFramebuffer) DCFlushRange(m_drcFramebuffer, m_drcSize);

    // Swap primario/secundario de ambos displays. OSScreenFlipBuffersEx
    // (SDK real) hace el flush de cache y actualiza los registros y
    // el estado del flip interno; el buffer que dibujamos pasa a visible.
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

} // namespace retroU
