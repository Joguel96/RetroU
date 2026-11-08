// =============================================================================
// main.cpp - Punto de entrada de RetroU (RPX nativo, entorno Aroma)
// =============================================================================
//
// CICLO DE VIDA CRITICO (verificado contra libwhb/proc.c + console.c, y
// contra el header INSTALADO de proc_ui/procui.h):
//
//   1. ProcUIInitEx(saveCb) -> el OS gestiona el foreground de la app.
//      SIN ESTO la app NUNCA adquiere foreground: pantalla negra + input
//      muerto (bug original).
//   2. renderer.init(): OSScreen + buffers MEM1 frame-heap + callbacks
//      ACQUIRE/RELEASE de ProcUI (patron libwhb/console.c).
//   3. Loop con ProcUIProcessMessages(TRUE) - cada estado:
//        IN_FOREGROUND       -> escaneo (1a vez) -> VPAD -> draw -> sleep
//        RELEASE_FOREGROUND  -> liberar buffers + ProcUIDrawDoneRelease()
//        IN_BACKGROUND       -> sin dibujo ni input; dormir (poco CPU en BG)
//        EXITING             -> el usuario cerro la app (menu HOME): salir
//   4. renderer.shutdown() + ProcUIShutdown() y fin del proceso.
//
// LECTURA CRITICA DEL HEADER (procui.h, lineas 207-218, verificado en
// C:/devkitPro/wut): con block=FALSE el valor de RETORNO ES UNDEFINED y el
// modo no bloqueante "is not widely used and may have undocumented
// behaviour". Usar FALSE + leer el retorno (bug de esta version) provocaba
// estados basura en el switch: la app entraba en RELEASE_FOREGROUND en
// bucle (sin sleep) martillando ProcUIDrawDoneRelease y nunca llegaba a
// IN_FOREGROUND -> sistema aparentemente congelado. POR ESO: solo TRUE
// (mismo patron que libwhb WHBProcIsRunning, probado en hardware).
//
// NOTAS DE API (headers instalados de WUT 1.9.2):
//   * ProcUIInitEx(ProcUISaveCallbackEx, void*) - proc_ui/procui.h:185.
//     Save callback canonico (libwhb/proc.c): OSSavesDone_ReadyToRelease().
//   * VPADInit() esta WUT_DEPRECATED, pero VPAD_READ_UNINITIALIZED del
//     MISMO header indica "need to call VPADInit()": fallback UNA vez si
//     la primera lectura lo reporta (pragma silencia el aviso).
//   * FS solo dentro de IN_FOREGROUND confirmado (regla de acceso del OS).
//   * OSSleepTicks (coreinit/thread.h:663) + OSMillisecondsToTicks.

#include <coreinit/foreground.h> // OSSavesDone_ReadyToRelease
#include <coreinit/thread.h>     // OSSleepTicks
#include <coreinit/time.h>       // OSMillisecondsToTicks
#include <proc_ui/procui.h>      // ProcUIInitEx / ProcessMessages / ...
#include <vpad/input.h>

#include "gfx/renderer.hpp"
#include "ui/menu.hpp"
#include "core/launcher.hpp"
#include "fs/scanner.hpp"

#include <cstdio>

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
        // VPADInit() esta WUT_DEPRECATED, pero el enum VPAD_READ_UNINITIALIZED
        // del mismo header indica que en este caso hace falta llamarlo:
        // se intenta UNA sola vez (no cada frame).
        static bool vpadInitTried = false;
        if (!vpadInitTried) {
            vpadInitTried = true;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            VPADInit();
#pragma GCC diagnostic pop

            VPADRead(VPAD_CHAN_0, outStatus, 1, &error);
        }
    }

    // VPAD_READ_NO_SAMPLES / VPAD_READ_BUSY no son errores fatales:
    // simplemente no hay input nuevo este frame.
    return error == VPAD_READ_SUCCESS;
}

// Banner de diagnostico de arranque: se dibuja y hace flip INMEDIATAMENTE
// para que, si la app se congela en el paso siguiente, el usuario vea el
// ultimo texto alcanzado. Se borra solo con el primer frame de menu.
void showStage(Renderer &r, const char *msg)
{
    r.clear(retroU::kColorBackground);
    r.drawText(2, 2, "RetroU - arranque");
    r.drawText(2, 4, msg);
    r.present();
}

} // namespace

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    // 1) Ciclo de vida ProcUI: prerequisito de renderer.init() y condicion
    //    para que el OS nos muestre en pantalla.
    ProcUIInitEx(&procSaveCallback, nullptr);

    // 2) Video: OSScreen + buffers MEM1 + callbacks ACQUIRE/RELEASE.
    Renderer renderer;
    if (!renderer.init()) {
        ProcUIShutdown();
        return 1;
    }
    showStage(renderer, "1/4 ProcUI + pantalla OK");

    // 3) GamePad: sonda previa (si se congela aqui, el culpable es VPAD).
    {
        VPADStatus probe{};
        showStage(renderer,
                  readGamepad(&probe) ? "2/4 GamePad OK"
                                      : "2/4 GamePad sin datos (normal)");
    }

    Scanner scanner;
    Launcher launcher;
    Menu menu(scanner);

    bool running          = true;
    bool frameDirty       = true;  // Repintar solo cuando cambie algo
    bool wasForeground    = false; // Detectar (re)entrada a foreground
    bool scanned          = false; // Escaneo FS en el 1er FOREGROUND

    // 4) Loop principal -------------------------------------------------------
    // TRUE = modo bloqueante SOPORTADO (el header advierte que con FALSE el
    // retorno es undefined y el modo no esta probado). Patron libwhb.
    while (running) {
        const ProcUIStatus status = ProcUIProcessMessages(TRUE);

        switch (status) {
        case PROCUI_STATUS_IN_FOREGROUND: {
            // (Re)entrada a foreground: el callback ACQUIRE realoco los
            // buffers con contenido basura -> forzar repintado completo.
            if (!wasForeground) {
                frameDirty    = true;
                wasForeground = true;
            }

            // Escaneo de ROMs solo con foreground confirmado por el OS
            // (regla de acceso: FS restringido en background). Banner
            // previo: si el scan se congela, queda visible en pantalla.
            if (!scanned) {
                showStage(renderer, "3/4 Escaneando ROMs...");
                char msg[64];
                if (scanner.scan()) {
                    std::snprintf(msg, sizeof(msg),
                                  "4/4 Scan OK: %u ROM(s)",
                                  static_cast<unsigned>(
                                      scanner.roms().size()));
                } else {
                    std::snprintf(msg, sizeof(msg),
                                  "4/4 Scan sin SD (menu con 0)");
                }
                showStage(renderer, msg);
                scanned = true;
                frameDirty = true; // el banner se reemplaza por el menu
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

    // 5) Cierre ordenado: buffers -> OSScreen -> ProcUI. Sin SYSLaunchMenu:
    //    en EXITING el SO ya gestiona la salida (patron libwhb).
    renderer.shutdown();
    ProcUIShutdown();
    return 0;
}
