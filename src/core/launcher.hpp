#pragma once

// =============================================================================
// launcher.hpp - Enlaza consolas seleccionadas con sus cores Libretro
// =============================================================================
//
// ESTADO: stub de fase 1. El objetivo final (AGENTS.md) es cargar cores
// Libretro (.rpl) como componentes dinamicos e invocar la API real:
//   retro_init / retro_run / retro_load_game + callbacks de
//   environment/video/audio/input.
// Cada core se compilara con wut_create_rpl (identico al main .rpx pero
// con el flag de RPL) y el escaneo de ROMs llega en la fase FS.

#include "ui/menu.hpp" // EmuConsole

namespace retroU {

class Launcher {
public:
    // Intenta arrancar la consola pedida. Devuelve true si lanzo algo;
    // en fase 1 siempre falla (sin cores todavia) y el menu muestra
    // el feedback.
    bool launch(EmuConsole console);

private:
    EmuConsole m_lastTarget = EmuConsole::Atari;
};

} // namespace retroU
