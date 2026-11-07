// =============================================================================
// launcher.cpp - Loader de cores Libretro (stub fase 1)
// =============================================================================
//
// PLAN (alineado a AGENTS.md):
//   Fase 2:
//     - Descubrir cores instalados (filesystem via coreinit FS*), un
//       core .rpl por consola.
//   Fase 3:
//     - Cargar el core dinamicamente y resolver exports de la API real:
//         retro_init / retro_run / retro_load_game / retro_get_system_*
//     - Proporcionar callbacks de environment/video/audio/input donde
//       GX2 presentara los framebuffers del core.
//   Nada de esto se implementa inventado: cada paso se verificara contra
//   libretro.h y los headers de WUT antes de codificarlo.

#include "core/launcher.hpp"
#include "ui/menu.hpp" // EmuConsole

namespace retroU {

bool Launcher::launch(EmuConsole console)
{
    // Guardamos el objetivo para el log futuro / consultas de estado.
    m_lastTarget = console;

    // Fase 1: sin cores cargables todavia. NO se simula exito: el menu
    // mostrara el feedback de "no disponible" a traves del valor false.
    return false;
}

} // namespace retroU
