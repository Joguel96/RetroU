#pragma once

// =============================================================================
// menu.hpp - UI de RetroU: lista de consolas + vista grid de juegos
// =============================================================================
//
// Dos vistas:
//   Consoles: lista de sistemas (D-Pad UP/DOWN, A abre la grid, HOME es
//             del OS via ProcUI). Muestra el conteo de ROMs escaneadas.
//   Games   : cuadrícula navegable (GameGrid) con los juegos del sistema.
//             A confirma (launcher), B vuelve a la lista.
//
// El enum EmuConsole vive en core/systems.hpp (compartido con fs/).

#include <cstdint>

#include "core/systems.hpp"
#include "ui/grid.hpp"

namespace retroU {

class Renderer;
class Launcher;
class Scanner;

class Menu {
public:
    // 'scanner' debe vivir tanto como el Menu (lo usa para conteos y para
    // cargar la grid al abrir un sistema).
    explicit Menu(const Scanner &scanner);

    // Procesa el input del frame (bits de trigger = pulsaciones "nuevas").
    // Devuelve true si el estado visual cambio (main.cpp repintara el frame).
    bool onInput(uint32_t pressed, uint32_t held, Launcher &launcher);

    // Dibuja la vista actual sobre el renderer compartido.
    void draw(Renderer &r);

private:
    enum class View : uint8_t {
        Consoles,
        Games,
    };

    const Scanner &m_scanner;
    View           m_view          = View::Consoles;
    uint32_t       m_selected      = 0; // indice resaltado en la lista
    EmuConsole     m_currentSystem = EmuConsole::Atari;
    GameGrid       m_grid;
    bool           m_launchFeedback   = false; // "core no disponible..."
    bool           m_emptyRomFeedback = false; // "sin juegos en <consola>"

    static const char *consoleName(uint32_t index);
};

} // namespace retroU
