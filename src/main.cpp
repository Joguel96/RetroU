// =============================================================================
// main.cpp - Punto de entrada de RetroU (RPX nativo, entorno Aroma)
// =============================================================================
//
// CICLO DE VIDA CRITICO (verificado contra libwhb/proc.c + console.c de WUT):
//
//   1. ProcUIInitEx(saveCb) -> el OS gestiona el foreground de la app.
//      SIN ESTO la app NUNCA adquiere foreground en hardware real: el OS no
//      muestra sus framebuffers ni enruta el input -> pantalla negra en TV y
//      GamePad y control sin respuesta (ni HOME). Fue el bug reportado.
//   2. renderer.init(): OSScreen + buffers del MEM1 frame-heap + registro de
//      callbacks ACQUIRE/RELEASE de ProcUI (patron libwhb/console.c).
//   3. Loop con ProcUIProcessMessages(FALSE) - cada estado:
//        IN_FOREGROUND       -> VPADRead -> update -> draw (si dirty) -> sleep
//        RELEASE_FOREGROUND  -> liberar buffers + ProcUIDrawDoneRelease()
//        IN_BACKGROUND       -> sin dibujo ni input; dormir (poco CPU en BG)
//        EXITING             -> el usuario cerro la app (menu HOME): salir
//   4. renderer.shutdown() + ProcUIShutdown() y fin del proceso. El OS
//      decide a donde volver; NO se fuerza SYSLaunchMenu en el cierre.
//
// NOTAS DE API (verificadas en los headers INSTALADOS de WUT 1.9.2):
//   * ProcUIInitEx(ProcUISaveCallbackEx, void*) - proc_ui/procui.h:185.
//     Save callback canonico (igual que libwhb/proc.c): llama
//     OSSavesDone_ReadyToRelease() (coreinit/foreground.h:21).
//   * ProcUIRegisterCallback lo hace renderer.init() (patron libwhb).
//   * VPADInit() esta WUT_DEPRECATED en vpad/input.h, pero el enum
//     VPAD_READ_UNINITIALIZED del MISMO header indica "need to call
//     VPADInit()": por eso existe el fallback que lo llama UNA vez si la
//     primera lectura lo reporta (pragma silencia el aviso de deprecacion).
//   * HOME: ahora lo gestiona el OS (overlay del menu HOME, comportamiento
//     estandar de app). El cierre real llega como PROCUI_STATUS_EXITING.
//   * OSSleepTicks (coreinit/thread.h:663) + OSMillisecondsToTicks
//     (coreinit/time.h:59) para pacing de frames.

#include <coreinit/foreground.h> // OSSavesDone_ReadyToRelease
#include <coreinit/thread.h>     // OSSleepTicks
#include <coreinit/time.h>       // OSMillisecondsToTicks
#include <proc_ui/procui.h>      // ProcUIInitEx / ProcessMessages / ...
#include <vpad/input.h>

#include "gfx/renderer.hpp"
#include "ui/menu.hpp"
#include "core/launcher.hpp"
#include "fs/scanner.hpp"

namespace {

using retroU::Renderer;
using retroU::Menu;
using retroU::Launcher;
using retroU::Scanner;

// Save callback canonico (mismo que libwhb/proc.c procSaveCallback): avisa
// al OS de que el estado de guardado esta listo para liberarse.
uint32_t procSaveCallback(void *context)
{
    (void)context;
    OSSavesDone_ReadyToRelease();
    return 0;
}

// Lee el GamePad (canal unico de la consola retail). Devuelve true si hay
// estado nuevo utilizable en outStatus.
bool readGamepad(VPADStatus *outStatus)
{
    VPADReadError error = VPAD_READ_INVALID_CONTROLLER;
    VPADRead(VPAD_CHAN_0, outStatus, 1, &error);

    if (error == VPAD_READ_UNINITIALIZED) {
        // Fallback unico: el header oficial dice que en este caso hace falta
        // VPADInit() (aunque WUT lo marque deprecated para uso normal).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        VPADInit();
#pragma GCC diagnostic pop

        VPADRead(VPAD_CHAN_0, outStatus, 1, &error);
    }

    // VPAD_READ_NO_SAMPLES / VPAD_READ_BUSY no son errores fatales:
    // simplemente no hay input nuevo este frame.
    return error == VPAD_READ_SUCCESS;
}

} // namespace

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    // 1) Ciclo de vida ProcUI: prerequisito de renderer.init() (registra
    //    callbacks) y condicion para que el OS nos muestre en pantalla.
    ProcUIInitEx(&procSaveCallback, nullptr);

    // 2) Video: OSScreen + buffers MEM1 + callbacks ACQUIRE/RELEASE.
    Renderer renderer;
    if (!renderer.init()) {
        // Sin video no hay frontend: se libera ProcUI y se termina; el OS
        // vuelve al contexto anterior (menu de Aroma).
        ProcUIShutdown();
        return 1;
    }

    // Escaneo de ROMs (sd:/retroU/roms/[sistema]/) una vez al arrancar:
    // estamos en foreground, momento valido para FS. Un fallo (SD ausente)
    // no es fatal: el menu mostrara "(0)" en cada sistema.
    Scanner scanner;
    scanner.scan();

    Menu menu(scanner);
    Launcher launcher;

    bool running          = true;
    bool frameDirty       = true;  // Repintar solo cuando cambie algo
    bool wasForeground    = false; // Detectar (re)entrada a foreground

    // 3) Loop principal -------------------------------------------------------
    while (running) {
        // FALSE = no bloquear: el pacing lo controlamos con OSSleepTicks
        // (en background dormimos mas; con TRUE dormiria por dentro el OS
        // y no tendriamos ese control).
        const ProcUIStatus status = ProcUIProcessMessages(FALSE);

        switch (status) {
        case PROCUI_STATUS_IN_FOREGROUND: {
            // (Re)entrada a foreground: el callback ACQUIRE realoco los
            // buffers con contenido basura -> forzar repintado completo.
            if (!wasForeground) {
                frameDirty    = true;
                wasForeground = true;
            }

            VPADStatus vpad{};
            if (readGamepad(&vpad)) {
                if (menu.onInput(vpad.trigger, vpad.hold, launcher)) {
                    frameDirty = true;
                }
            }

            // OSScreenPutPixelEx es por pixel: repintar sin cambios gastaria
            // CPU que necesitaremos para los cores Libretro (frame "dirty").
            if (frameDirty) {
                renderer.clear();
                menu.draw(renderer);
                renderer.present();
                frameDirty = false;
            }
            // Si no hay cambios NO se flippea: el display conserva el ultimo
            // frame visible.

            // Pacing ~70 fps max: cede el hilo al scheduler y evita
            // bombardear VPADRead.
            OSSleepTicks(OSMillisecondsToTicks(14));
            break;
        }

        case PROCUI_STATUS_RELEASE_FOREGROUND:
            // El OS pide volver a background (HOME menu / cambio de app).
            // onRelease() es idempotente: si el callback RELEASE ya corrio
            // dentro de ProcessMessages, aqui es un no-op.
            renderer.onRelease();
            ProcUIDrawDoneRelease(); // confirma al OS el release (libwhb)
            wasForeground = false;
            break;

        case PROCUI_STATUS_IN_BACKGROUND:
            // Sin graficos ni input en background: el OS asigna poco CPU
            // a las apps en BG; dormir generosamente.
            wasForeground = false;
            OSSleepTicks(OSMillisecondsToTicks(50));
            break;

        case PROCUI_STATUS_EXITING:
            // El usuario cerro RetroU desde el menu HOME: liberar todo y
            // terminar. El OS decide el titulo al que volver.
            running = false;
            break;
        }
    }

    // 4) Cierre ordenado: buffers -> OSScreen -> ProcUI. Sin SYSLaunchMenu:
    //    en EXITING el SO ya gestiona la salida (patron libwhb).
    renderer.shutdown();
    ProcUIShutdown();
    return 0;
}
