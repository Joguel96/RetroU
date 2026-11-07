#pragma once

// =============================================================================
// menu.hpp - Menu principal de RetroU: seleccion de consola retro
// =============================================================================
//
// Lista de sistemas objetivo (AGENTS.md): Atari, MSX, NES, SNES, Genesis,
// GBA, PS1, Arcade. Navegacion con D-Pad (UP/DOWN), confirmacion con A,
// cancelacion con B. Boton HOME lo procesa main.cpp (no aqui).

#include <cstdint>

namespace retroU {

class Renderer;
class Launcher;

// Consolas a emular (orden del menu). El indice ES el id del core que
// asociara Launcher cuando existan cores Libretro cargables.
enum class EmuConsole : uint32_t {
    Atari   = 0,
    MSX     = 1,
    NES     = 2,
    SNES    = 3,
    Genesis = 4,
    GBA     = 5,
    PS1     = 6,
    Arcade  = 7,
    Count   = 8,
};

class Menu {
public:
    // Procesa el input del frame (bits de trigger = pulsaciones "nuevas").
    // 'launcher' se usa al confirmar (A). Devuelve true si el estado
    // visual del menu cambio (main.cpp repintara el frame).
    bool onInput(uint32_t pressed, uint32_t held, Launcher &launcher);

    // Dibuja el estado actual del menu sobre el renderer compartido.
    void draw(Renderer &r);

private:
    uint32_t m_selected = 0;          // indice resaltado en la lista
    bool     m_launchFeedback = false; // feedback "cargando..." al confirmar

    static const char *consoleName(uint32_t index);
};

} // namespace retroU
