#pragma once

// =============================================================================
// systems.hpp - Sistemas objetivo de RetroU (enum compartido)
// =============================================================================
//
// El enum vive aqui (no en menu.hpp) para que los modulos de datos (fs/) no
// dependan de la capa UI. Orden identico al del menu original: el indice ES
// el id del core que asociara Launcher cuando existan cores Libretro.

#include <cstdint>

namespace retroU {

enum class EmuConsole : uint32_t {
    Atari   = 0,
    MSX     = 1,
    NES     = 2,
    SNES    = 3,
    Genesis = 4,
    GBA     = 5,
    PS1     = 6,
    Arcade  = 7,
    Count   = 8, // valor sentinela: "sistema desconocido"
};

} // namespace retroU
