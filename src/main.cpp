// =============================================================================
// main.cpp - Punto de entrada de RetroU (RPX nativo, entorno Aroma)
// =============================================================================
//
// CICLO DE VIDA VERIFICADO (WUT + WiiUBrew):
//   1. OSScreenInit + buffers + enable    -> subsistema de pantalla
//   2. Loop: VPADRead (GamePad) -> update -> draw -> flush/flip -> sleep
//   3. HOME o opcion "Salir" -> SYSLaunchMenu() (sysapp/launch.h),
//      devolviendo el control al menu de Horizon OS de forma limpia.
//
// NOTAS DE API (verificadas, NO inventadas):
//   * VPADInit()/VPADShutdown() estan marcados WUT_DEPRECATED en
//     include/vpad/input.h; en el entorno actual VPAD basta con leerlo.
//   * VPADRead llena VPADStatus; usamos status.trigger (pulsaciones
//     "nuevas" este frame) y status.hold (estado mantenido).
//   * Botones: VPAD_BUTTON_HOME=0x0002, A=0x8000, B=0x4000,
//     UP=0x0200 DOWN=0x0100 LEFT=0x0800 RIGHT=0x0400.
//   * OSSleepTicks + OSMillisecondsToTicks para ceder CPU entre frames
//     (coreinit/thread.h + coreinit/time.h).

#include <coreinit/thread.h> // OSSleepTicks
#include <coreinit/time.h>   // OSMillisecondsToTicks
#include <vpad/input.h>
#include <sysapp/launch.h>

#include "gfx/renderer.hpp"
#include "ui/menu.hpp"
#include "core/launcher.hpp"

namespace {

using retroU::Renderer;
using retroU::Menu;
using retroU::Launcher;

bool readGamepad(VPADStatus *outStatus, bool *outExitRequested)
{
    // Solo el canal 0: la consola retail tiene un unico GamePad.
    VPADReadError error = VPAD_READ_INVALID_CONTROLLER;
    VPADRead(VPAD_CHAN_0, outStatus, 1, &error);

    if (error != VPAD_READ_SUCCESS) {
        // VPAD_READ_NO_SAMPLES y VPAD_READ_BUSY no son errores fatales:
        // simplemente no hay input nuevo este frame.
        return false;
    }

    // Deteccion de "trigger": boton nuevo (buffer edge). HOME pide salida.
    if (outStatus->trigger & VPAD_BUTTON_HOME) {
        *outExitRequested = true;
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    Renderer renderer;
    if (!renderer.init()) {
        // Sin video no hay frontend: no es recuperable de forma segura a
        // este nivel; se solicita volver al menu de Horizon y se abandona.
        SYSLaunchMenu();
        return 1;
    }

    Menu menu;
    Launcher launcher;

    bool exitRequested = false;
    bool frameDirty    = true;  // Repintar solo cuando cambie algo en pantalla

    while (!exitRequested) {
        VPADStatus vpad{};
        const bool hasInput = readGamepad(&vpad, &exitRequested);

        if (hasInput) {
            const bool changed = menu.onInput(vpad.trigger, vpad.hold, launcher);
            if (changed) {
                frameDirty = true;
            }
        }

        // Solo redibujamos si la UI cambio: OSScreenPutPixelEx es una
        // llamada por pixel; dibujar 60 veces por seg sin cambios gastaria
        // CPU que necesitamos para los cores Libretro.
        if (frameDirty) {
            renderer.clear();
            menu.draw(renderer);
            renderer.present();
            frameDirty = false;
        }
        // Si no hay cambios NO se flippea: el display conserva el ultimo
        // frame y el proximo redibujo parte del buffer alternado, cuyo
        // contenido obsoleto se elimina con clear() antes de escribir.

        // Frame pacing: dormir ~14 ms deja margen para el dibujo y evita
        // bombardeo de VPADRead; OSSleepTicks cede el hilo al scheduler.
        OSSleepTicks(OSMillisecondsToTicks(14));
    }

    // Cierre ordenado: liberar video ANTES de devolver el control al SO,
    // para que el menu de Horizon recupere sus framebuffers sin estados
    // intermedios del frontend.
    renderer.shutdown();

    // Salida limpia al menu de Horizon OS (sysapp). SYSLaunchMenu no mata
    // el proceso: el titulo sigue vivo hasta que el usuario elige otra
    // cosa, por eso devolvemos sin mas trabajo pendiente.
    SYSLaunchMenu();
    return 0;
}
