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
// Todos los metodos operan sobre el TV y el GamePad simultaneamente.
// ---------------------------------------------------------------------------
class Renderer {
public:
    // Resoluciones nativas (TV y GamePad)
    static constexpr uint32_t TV_WIDTH   = 1280;
    static constexpr uint32_t TV_HEIGHT  = 720;
    static constexpr uint32_t DRC_WIDTH  = 854;
    static constexpr uint32_t DRC_HEIGHT = 480;

    // Inicializa OSScreen y reserva los framebuffers de TV y DRC
    // (con OSAllocFromSystem; OSScreenSetBufferEx exige alineacion 0x100,
    // ver WiiUBrew/Coreinit.rpl).
    // Devuelve false si el sistema de video no estuvo disponible.
    bool init();

    // Libera framebuffers y apaga OSScreen. Idempotente.
    void shutdown();

    // Limpia los work buffers de ambas pantallas en un color solido
    // (OSScreenClearBufferEx del SDK real: copia u32-a-u32 del color
    // en todo el framebuffer, sin necesitar shaders).
    void clear(uint32_t color = kColorBackground);

    // Relleno de rectangulo solido (barra de seleccion del menu, paneles).
    // NOTA DE RENDIMIENTO: va pixel a pixel via OSScreenPutPixelEx.
    // Patron correcto con OSScreen: marcar frame "dirty" solo cuando algo
    // cambia (ver main.cpp) y repintar solo entonces.
    void drawRect(int x, int y, int w, int h, uint32_t color);

    // Texto con la fuente monospace integrada de OSScreen.
    // OSScreenPutFontEx recibe la posicion en CELDAS de caracter, NO en
    // pixeles (columnas ~ como x/12, filas ~ y/24; ver docs del header).
    // Texto blanco anti-aliased incorporado; no hace falta bitmap font.
    void drawText(uint32_t column, uint32_t row, const char *text);

    // Presenta los buffers dibujados: swap de ambos displays (TV + DRC).
    // Llamar UNA vez por frame, DESPUES de todo el dibujo.
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
};

} // namespace retroU
