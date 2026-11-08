// =============================================================================
// renderer.cpp - Implementacion del backend OSScreen (fase 1 de RetroU)
// =============================================================================
//
// TODAS las llamadas usadas aqui estan verificadas contra:
//   - WUT include/coreinit/{screen,memory,cache,time,memfrmheap,memheap}.h
//     (descargados y revisados firma por firma; NO inventados)
//   - WiiUBrew "Coreinit.rpl" seccion Screen (semantica de hardware real)
//   - decaf (emulador Wii U) coreinit_screen.cpp (firmas/formatos)
//   - libwhb (WUT) console.c  y  libwiiu draw.c (patrones canonicos)
//
// NOTA DE VIDA CRITICA (hardware real): los framebuffers se asignan desde el
// MEM1 frame-heap y se liberan en el release de foreground, tal como hace
// libwhb/console.c. La consola usa ese MEM1 mientras la app esta en
// background; dibujar (o incluso tener buffers) en background rompe el
// entorno. OSAllocFromSystem NO es una alternativa valida: el buffer de TV
// (2 * 0x384000 = 0x708000) excede el system heap.

#include "gfx/renderer.hpp"

#include <coreinit/screen.h>    // OSScreen* (SCREEN_TV/SCREEN_DRC, funcs)
#include <coreinit/cache.h>     // DCFlushRange (cache -> memoria antes del flip)
#include <coreinit/memfrmheap.h>// MEMAllocFromFrmHeapEx, MEMRecordState...
#include <coreinit/memheap.h>   // MEMGetBaseHeapHandle, MEM_BASE_HEAP_MEM1
#include <proc_ui/procui.h>     // ProcUIRegisterCallback (ACQUIRE/RELEASE)

namespace retroU {

namespace {

// Tag unico para recordar/liberar todo el bloque de buffers de RetroU en el
// frame-heap de una sola pieza (mismo patron que libwhb/console.c usa con
// su propio tag 0x000DECAF).
constexpr uint32_t kFrmHeapTag = 0x5245544FU; // "RETO"

// Trampolines para los callbacks de ProcUI (mismo patron que
// libwhb/console.c: ConsoleProcCallbackAcquired/Released). El OS los invoca
// dentro de ProcUIProcessMessages cuando RetroU entra/sale de foreground;
// el puntero 'context' es la instancia de Renderer.
uint32_t rendererAcquired(void *context)
{
    static_cast<Renderer *>(context)->onAcquire();
    return 0;
}

uint32_t rendererReleased(void *context)
{
    static_cast<Renderer *>(context)->onRelease();
    return 0;
}

} // namespace

bool Renderer::init()
{
    // REQUISITO: ProcUIInitEx() debe haberse llamado antes (ver main.cpp):
    // ProcUIRegisterCallback exige la libreria ProcUI inicializada.

    // 1) Inicializa el subsistema de pantalla de coreinit (activa registros
    //    de framebuffer internos de GX2 desde el SDK real). Se llama UNA
    //    vez, antes de que ProcUI nos entregue el foreground.
    OSScreenInit();

    // 2) Cachea los tamanos (TOTAL = primario + secundario por pantalla).
    m_tvSize  = OSScreenGetBufferSizeEx(SCREEN_TV);
    m_drcSize = OSScreenGetBufferSizeEx(SCREEN_DRC);

    // 3) Adquisicion inicial: la app arranca en foreground, asi que se
    //    piden los buffers YA (patron de WHBLogConsoleInit en libwhb).
    onAcquire();

    // 4) Registra los callbacks de transicion para que el OS gestione
    //    buffers en cada cambio de foreground (HOME menu, cambio de app).
    //    Prioridad 100 = la misma que usa libwhb.
    ProcUIRegisterCallback(PROCUI_CALLBACK_ACQUIRE, &rendererAcquired, this, 100);
    ProcUIRegisterCallback(PROCUI_CALLBACK_RELEASE, &rendererReleased, this, 100);
    return true;
}

void Renderer::shutdown()
{
    // Si seguimos en foreground (cierre inesperado), liberamos primero.
    if (m_foreground) {
        onRelease();
    }

    // OSScreenShutdown() del SDK real solo hace EnableEx(0/1, false).
    OSScreenShutdown();
}

void Renderer::onAcquire()
{
    if (m_foreground) return; // ya adquirido (idempotente)

    // 3) Reserva los buffers en el MEM1 frame-heap, exactamente como
    //    libwhb/console.c (alineacion 4 como en la referencia; el frame
    //    heap ya provee la alineacion efectiva que OS exige para
    //    OSScreenSetBufferEx, 0x100, por los bloques del heap).
    MEMHeapHandle heap = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM1);
    MEMRecordStateForFrmHeap(heap, kFrmHeapTag);

    m_tvFramebuffer  = MEMAllocFromFrmHeapEx(heap, m_tvSize,  4);
    m_drcFramebuffer = MEMAllocFromFrmHeapEx(heap, m_drcSize, 4);

    if (!m_tvFramebuffer || !m_drcFramebuffer) {
        // Sin memoria no hay pantalla: no avanzamos (no se dibuja nada).
        MEMFreeByStateToFrmHeap(heap, kFrmHeapTag);
        m_tvFramebuffer = m_drcFramebuffer = nullptr;
        return;
    }

    // 4) Registra las areas y habilita ambos displays.
    OSScreenSetBufferEx(SCREEN_TV,  m_tvFramebuffer);
    OSScreenSetBufferEx(SCREEN_DRC, m_drcFramebuffer);
    OSScreenEnableEx(SCREEN_TV,  1);
    OSScreenEnableEx(SCREEN_DRC, 1);

    m_foreground = true;
}

void Renderer::onRelease()
{
    if (!m_foreground) return;

    // 5) Devuelve TODO el bloque al frame-heap en una sola operacion
    //    (libwhb/libwiiu lo hacen asi; el OS reasignara ese MEM1).
    MEMHeapHandle heap = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM1);
    MEMFreeByStateToFrmHeap(heap, kFrmHeapTag);
    m_tvFramebuffer = m_drcFramebuffer = nullptr;
    m_foreground = false;

    // Deshabilita el display: el OS mostrara negro / otro titulo.
    OSScreenEnableEx(SCREEN_TV,  0);
    OSScreenEnableEx(SCREEN_DRC, 0);
}

void Renderer::clear(uint32_t color)
{
    if (!m_foreground) return;
    OSScreenClearBufferEx(SCREEN_TV,  color);
    OSScreenClearBufferEx(SCREEN_DRC, color);
}

void Renderer::drawRect(int x, int y, int w, int h, uint32_t color)
{
    if (!m_foreground || w <= 0 || h <= 0) return;

    // Coordenadas virtuales: el sistema "logico" del UI es 1280x720 (TV).
    // El mismo rect se dibuja en cada display en su resolucion nativa,
    // escalando una vez por display (no pixel-a-pixel cruzado).
    const int x1 = (x < 0) ? 0 : x;
    const int y1 = (y < 0) ? 0 : y;
    const int x2 = x + w;
    const int y2 = y + h;

    // Clipping + escritura en el TV (1280x720, coords 1:1)
    for (int py = y1; py < y2; ++py) {
        if (py < 0 || py >= static_cast<int>(TV_HEIGHT)) continue;
        for (int px = x1; px < x2; ++px) {
            if (px < 0 || px >= static_cast<int>(TV_WIDTH)) continue;
            OSScreenPutPixelEx(SCREEN_TV, px, py, color);
        }
    }

    // Mismo rect escalado al gamepad (854x480)
    const int dx1 = x1 * static_cast<int>(DRC_WIDTH)  / static_cast<int>(TV_WIDTH);
    const int dy1 = y1 * static_cast<int>(DRC_HEIGHT) / static_cast<int>(TV_HEIGHT);
    const int dx2 = x2 * static_cast<int>(DRC_WIDTH)  / static_cast<int>(TV_WIDTH);
    const int dy2 = y2 * static_cast<int>(DRC_HEIGHT) / static_cast<int>(TV_HEIGHT);

    for (int py = dy1; py < dy2; ++py) {
        if (py < 0 || py >= static_cast<int>(DRC_HEIGHT)) continue;
        for (int px = dx1; px < dx2; ++px) {
            if (px < 0 || px >= static_cast<int>(DRC_WIDTH)) continue;
            OSScreenPutPixelEx(SCREEN_DRC, px, py, color);
        }
    }
}

void Renderer::drawText(uint32_t column, uint32_t row, const char *text)
{
    if (!m_foreground || !text) return;
    OSScreenPutFontEx(SCREEN_TV,  column, row, text);
    // En el gamepad usamos la misma cuadricula: la fuente interna escala
    // igual (posiciones en celdas de caracter) en ambos displays.
    OSScreenPutFontEx(SCREEN_DRC, column, row, text);
}

void Renderer::present()
{
    if (!m_foreground) return;

    // Flush del cache de datos de las (grandes) areas de framebuffer a
    // memoria fisica ANTES del flip: practica canonica de libwhb/console.c.
    // Sin esto, los ultimos writes de drawRect pueden no llegar a GDDR.
    if (m_tvFramebuffer)  DCFlushRange(m_tvFramebuffer,  m_tvSize);
    if (m_drcFramebuffer) DCFlushRange(m_drcFramebuffer, m_drcSize);

    // Swap primario/secundario de ambos displays. OSScreenFlipBuffersEx
    // (SDK real) hace el flush de cache y actualiza los registros y
    // el estado del flip interno; el buffer que dibujamos pasa a visible.
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

} // namespace retroU
