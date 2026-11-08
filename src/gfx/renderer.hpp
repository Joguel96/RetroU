#pragma once

// =============================================================================
// renderer.hpp - Backend de render para el frontend (fase inicial: OSScreen)
// =============================================================================
//
// DECISION DE ARQUITECTURA (ver AGENTS.md - solo APIs verificadas):
//
//   La fase 1 de RetroU usa OSScreen (coreinit/screen.h), el sistema de
//   render por software de Cafe OS. Motivos verificables:
//     * Es la via canonica de homebrew 2D estable hoy (libwhb de WUT la usa
//       internamente para su consola de log).
//     * El render nativo GX2 exige pares de shaders compilados offline
//       (binarios .gsh con cafeglsl/deflect); no pueden generarse aqui sin
//       ese pipeline. Cuando necesitemos sprites/texturas VRAM se
//       reemplazara el backend bajo esta misma interfaz.
//   OSScreen de todos modos usa los framebuffers reales que GX2 presenta
//   por TV (1280x720) y GamePad (854x480).
//
// CICLO DE VIDA (critico en hardware real, verificado contra libwhb/console.c):
//
//   La consola Wii U controla el foreground de la app via ProcUI. Sin
//   ProcUIInit + ProcUIProcessMessages la app NUNCA adquiere el foreground:
//   el OS no muestra sus buffers ni enruta el input (pantallas negras y
//   control sin respuesta). Por eso el renderer expone onAcquire/onRelease:
//
//     onAcquire():   MEM1 frame-heap (como libwhb) -> buffers -> SetBufferEx
//                    -> EnableEx(true)
//     onRelease():   MEMFreeByStateToFrmHeap (el OS puede usar MEM1 en
//                    background) -> buffers liberados
//
//   TODOS los dibujos son no-ops si !isForeground().
//
// FORMATO DE COLOR (verificado contra WiiUBrew/Coreinit.rpl, decaf y
// libwiiu/draw.c):
//   Pixel = uint32 big-endian dispuesto en memoria como bytes R,G,B,A:
//     (r << 24) | (g << 16) | (b << 8) | a  ==  0xRRGGBBAA
//
// RESOLUCIONES (tabla sScreenSizes de decaf, identica al SDK real):
//   TV : 1280 x 720, stride 1280 px
//   DRC: 854  x 480, stride visible 854 px (el frame buffer interno del
//        gamepad avanza 896 px por fila; solo relevante si en el futuro
//        se hace escritura directa al puntero de framebuffer)

#include <cstdint>

namespace retroU {

// Colores del frontend, formato 0xRRGGBBAA
inline constexpr uint32_t kColorBackground   = 0x101018FF;
inline constexpr uint32_t kColorText         = 0xE8E8E8FF;
inline constexpr uint32_t kColorHighlight    = 0xC86432FF; // accent retro
inline constexpr uint32_t kColorTextHighlight = 0xFFFFFFFF;

// ---------------------------------------------------------------------------
// Renderer: ciclo de vida de OSScreen + primitivas de dibujado.
// ---------------------------------------------------------------------------
class Renderer {
public:
    // Resoluciones nativas (TV y GamePad)
    static constexpr uint32_t TV_WIDTH   = 1280;
    static constexpr uint32_t TV_HEIGHT  = 720;
    static constexpr uint32_t DRC_WIDTH  = 854;
    static constexpr uint32_t DRC_HEIGHT = 480;

    // Inicializa OSScreen (una sola vez), cachea los tamanos de los
    // framebuffers, hace la adquisicion inicial de buffers (la app arranca
    // en foreground) y registra los callbacks ACQUIRE/RELEASE de ProcUI.
    // REQUISITO: ProcUIInitEx() debe haberse llamado antes (main.cpp).
    // Devuelve false si el subsistema de video no estuvo disponible.
    bool init();

    // Libera buffers (si los tiene) y apaga OSScreen. Idempotente.
    void shutdown();

    // --- Transiciones de foreground (llamar desde el loop de ProcUI) ---

    // Adquiere el foreground: reserva los framebuffers en el MEM1 frame-heap
    // (patron verificado de libwhb/console.c; OSAllocFromSystem no sirve
    // porque el TV necesita ~7.4MB y el system heap es mas pequeno),
    // registra los buffers en OSScreen y habilita ambos displays.
    void onAcquire();

    // Libera el foreground: devuelve los buffers al frame-heap (el OS
    // puede reusar ese MEM1 mientras la app esta en background) y
    // deshabilita el display.
    void onRelease();

    // true solo mientras el OS nos tiene en foreground (se puede dibujar).
    bool isForeground() const { return m_foreground; }

    // --- Primitivas (todas no-op si no hay foreground) ---

    // Limpia los work buffers de ambas pantallas en un color solido
    // (OSScreenClearBufferEx: copia u32-a-u32 del color en todo el
    // framebuffer, sin necesitar shaders).
    void clear(uint32_t color = kColorBackground);

    // Relleno de rectangulo solido (barra de seleccion, paneles).
    // NOTA DE RENDIMIENTO: va pixel a pixel via OSScreenPutPixelEx.
    // Patron correcto con OSScreen: marcar frame "dirty" solo cuando algo
    // cambia (ver main.cpp) y repintar solo entonces.
    void drawRect(int x, int y, int w, int h, uint32_t color);

    // Texto con la fuente monospace integrada de OSScreen.
    // OSScreenPutFontEx recibe la posicion en CELDAS de caracter, NO en
    // pixeles (ver docs del header de WUT). Texto blanco anti-aliased
    // incorporado; no hace falta bitmap font.
    void drawText(uint32_t column, uint32_t row, const char *text);

    // Presenta los buffers dibujados: flush del cache + swap de ambos
    // displays (TV + DRC). Llamar UNA vez por frame, DESPUES de todo el
    // dibujo.
    void present();

    // Asistente estatico de color: empaqueta el formato 0xRRGGBBAA.
    static constexpr uint32_t makeColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        return (uint32_t(r) << 24) | (uint32_t(g) << 16) |
               (uint32_t(b) << 8) |  uint32_t(a);
    }

private:
    void    *m_tvFramebuffer  = nullptr;
    void    *m_drcFramebuffer = nullptr;
    uint32_t m_tvSize         = 0;
    uint32_t m_drcSize        = 0;
    bool     m_foreground     = false;
};

} // namespace retroU
