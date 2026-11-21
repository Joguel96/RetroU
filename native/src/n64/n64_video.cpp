#include "n64_video.h"

#include "gx2/gx2_renderer.hpp"
#include "gx2/gx2_text.hpp"
#include "video_ui.h"
#include "report.h"

#include <coreinit/cache.h>
#include <coreinit/memdefaultheap.h>
#include <gx2/context.h>
#include <gx2/draw.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/state.h>
#include <gx2/surface.h>
#include <gx2/swap.h>
#include <gx2/texture.h>
#include <cstring>

extern const unsigned char _binary_ui_ttf_start[];
extern const unsigned char _binary_ui_ttf_end[];
namespace gx2 { void restoreState(); }

namespace {
GX2ContextState *s_copy_context;
GX2ColorBuffer s_shaped;
bool s_picture_43 = true;
bool s_menu_ready = false;
bool s_menu_drawn = false;

retrou::gx2::Color menuColor(const uint8_t rgba[4])
{
    return {rgba[0], rgba[1], rgba[2], rgba[3]};
}

void menuOnGraphicsCore(void *argument)
{
    using namespace retrou::gx2;
    const auto &view = *static_cast<const N64MenuView *>(argument);
    s_menu_drawn = false;
    if (!s_menu_ready) {
        video_set_hold(true);
        if (!attach()) {
            video_set_hold(false);
            report_line("N64: no se pudo adjuntar el renderizador del menu");
            return;
        }
        if (!text::loadFont(_binary_ui_ttf_start,
                            static_cast<size_t>(_binary_ui_ttf_end - _binary_ui_ttf_start))) {
            shutdown();
            video_set_hold(false);
            gx2::restoreState();
            report_line("N64: no se pudo cargar la fuente del menu");
            return;
        }
        const char *labels[] = {"MENU", "Continuar", "Guardar estado", "Cargar estado",
                                "Controles", "Salir del juego", "A  Aceptar    B  Volver",
                                "Slot 1", "Slot 2", "Slot 3", "Slot 4",
                                "Los controles se configuran en Ajustes"};
        for (const char *label : labels) text::prewarm(label, 20);
        text::prewarm("MENU", 26);
        text::prewarm("Guardar estado", 26);
        text::prewarm("Cargar estado", 26);
        text::prewarm("Controles", 26);
        text::prewarm("A  Aceptar    B  Volver", 15);
        s_menu_ready = true;
    }
    setOutputMode(view.output_mode);
    const Color white{255, 255, 255, 255};
    const Color muted{148, 163, 184, 255};
    const Color panel{21, 27, 40, 245};
    const Color accent = menuColor(view.accent);
    beginFrame(menuColor(view.background));
    drawGradient(0, 0, 1280, 720, menuColor(view.background_top),
                 menuColor(view.background));
    if (view.page == 0) {
        drawRoundRect(390, 170, 500, 380, 18, panel);
        text::drawText("MENU", 430, 200, 26, white);
        const char *items[] = {"Continuar", "Guardar estado", "Cargar estado",
                               "Controles", "Salir del juego"};
        for (int i = 0; i < 5; ++i)
            text::drawText(items[i], 430, 270 + i * 44, 20,
                           i == view.selected ? accent : muted);
        text::drawText("A  Aceptar    B  Volver", 430, 500, 15, muted);
    } else {
        drawRoundRect(290, 110, 700, 500, 18, panel);
        const char *title = view.page == 1 ? "Guardar estado" :
                            view.page == 2 ? "Cargar estado" : "Controles";
        text::drawText(title, 330, 138, 26, white);
        if (view.page == 3) {
            text::drawText("Los controles se configuran en Ajustes", 330, 260, 20, muted);
        } else {
            const char *slots[] = {"Slot 1", "Slot 2", "Slot 3", "Slot 4"};
            for (int i = 0; i < 4; ++i) {
                const float y = 205 + i * 88;
                if (i == view.slot)
                    drawRoundRect(315, y - 10, 650, 78, 10, Color{245, 217, 127, 40});
                text::drawText(slots[i], 455, y + 18, 20, i == view.slot ? accent : muted);
            }
        }
        text::drawText("A  Aceptar    B  Volver", 330, 570, 15, muted);
    }
    endFrame();
    s_menu_drawn = true;
    gx2::restoreState();
}

void hideMenuOnGraphicsCore(void *)
{
    if (s_menu_ready) {
        GX2DrawDone();
        retrou::gx2::text::unloadFont();
        retrou::gx2::shutdown();
        s_menu_ready = false;
    }
    video_set_hold(false);
    gx2::restoreState();
}

bool alloc_buffer(GX2ColorBuffer *buffer, uint32_t width, uint32_t height)
{
    std::memset(buffer, 0, sizeof(*buffer));
    buffer->surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    buffer->surface.width = width;
    buffer->surface.height = height;
    buffer->surface.depth = 1;
    buffer->surface.mipLevels = 1;
    buffer->surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    buffer->surface.aa = GX2_AA_MODE1X;
    buffer->surface.use = GX2_SURFACE_USE_TEXTURE_COLOR_BUFFER_TV;
    buffer->surface.tileMode = GX2_TILE_MODE_DEFAULT;
    buffer->viewNumSlices = 1;
    GX2CalcSurfaceSizeAndAlignment(&buffer->surface);
    GX2InitColorBufferRegs(buffer);
    buffer->surface.image = MEMAllocFromDefaultHeapEx(buffer->surface.imageSize,
                                                       buffer->surface.alignment);
    return buffer->surface.image != nullptr;
}

bool copy_context()
{
    if (s_copy_context) return true;
    s_copy_context = static_cast<GX2ContextState *>(
        MEMAllocFromDefaultHeapEx(sizeof(GX2ContextState), GX2_CONTEXT_STATE_ALIGNMENT));
    if (!s_copy_context) return false;
    GX2SetupContextStateEx(s_copy_context, TRUE);
    return true;
}

GX2ColorBuffer *shape(GX2ColorBuffer *frame)
{
    const uint32_t width = frame->surface.width;
    const uint32_t height = frame->surface.height;
    const uint32_t wide = (height * 16 / 9 + 1) & ~1u;
    if (!s_picture_43 || wide <= width || !copy_context()) return frame;
    if (s_shaped.surface.image &&
        (s_shaped.surface.width != wide || s_shaped.surface.height != height)) {
        MEMFreeToDefaultHeap(s_shaped.surface.image);
        s_shaped.surface.image = nullptr;
    }
    if (!s_shaped.surface.image) {
        if (!alloc_buffer(&s_shaped, wide, height)) return frame;
        std::memset(s_shaped.surface.image, 0, s_shaped.surface.imageSize);
        DCFlushRange(s_shaped.surface.image, s_shaped.surface.imageSize);
    }
    GX2Rect source = {0, 0, static_cast<int32_t>(width), static_cast<int32_t>(height)};
    GX2Point destination = {static_cast<int32_t>((wide - width) / 2), 0};
    GX2SetContextState(s_copy_context);
    GX2CopySurfaceEx(&frame->surface, 0, 0, &s_shaped.surface, 0, 0, 1,
                     &source, &destination);
    GX2Flush();
    return &s_shaped;
}
}

extern "C" int n64_video_menu_show(const N64MenuView *view)
{
    if (!view) return 0;
    s_menu_drawn = false;
    video_call(menuOnGraphicsCore, const_cast<N64MenuView *>(view));
    return s_menu_drawn ? 1 : 0;
}

extern "C" void n64_video_menu_hide(void)
{
    if (s_menu_ready) video_call(hideMenuOnGraphicsCore, nullptr);
}

GX2ColorBuffer *ui_gfx_overlay(GX2ColorBuffer *frame) { return frame; }

void ui_gfx_loading_progress(unsigned done, unsigned total)
{
    if (total && (done == 0 || done + 1 == total))
        report_line("N64: preparando graficos %u/%u", done + 1, total);
}

void ui_gfx_scan(GX2ColorBuffer *frame)
{
    if (!frame) return;
    frame = shape(frame);
    GX2CopyColorBufferToScanBuffer(frame, GX2_SCAN_TARGET_TV);
    GX2CopyColorBufferToScanBuffer(frame, GX2_SCAN_TARGET_DRC);
    GX2SwapScanBuffers();
}

void ui_gfx_screens_changed() {}
void ui_gfx_set_picture_43(bool enabled) { s_picture_43 = enabled; }

bool ui_gfx_copy_frame(GX2ColorBuffer *frame, GX2ColorBuffer *to)
{
    if (!frame || !to || !copy_context()) return false;
    if (to->surface.image &&
        (to->surface.width != frame->surface.width ||
         to->surface.height != frame->surface.height)) {
        MEMFreeToDefaultHeap(to->surface.image);
        to->surface.image = nullptr;
    }
    if (!to->surface.image &&
        !alloc_buffer(to, frame->surface.width, frame->surface.height))
        return false;
    GX2SetContextState(s_copy_context);
    GX2CopySurface(&frame->surface, 0, 0, &to->surface, 0, 0);
    GX2Invalidate(GX2_INVALIDATE_MODE_TEXTURE, to->surface.image,
                  to->surface.imageSize);
    GX2Flush();
    return true;
}

extern "C" void n64_video_release()
{
    if (s_shaped.surface.image) {
        MEMFreeToDefaultHeap(s_shaped.surface.image);
        s_shaped.surface.image = nullptr;
    }
    if (s_copy_context) {
        MEMFreeToDefaultHeap(s_copy_context);
        s_copy_context = nullptr;
    }
}
