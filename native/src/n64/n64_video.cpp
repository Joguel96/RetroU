#include "n64_video.h"

#include "gx2/gx2_renderer.hpp"
#include "gx2/gx2_text.hpp"
#include "video_ui.h"
#include "report.h"

#include <coreinit/cache.h>
#include <proc_ui/procui.h>
#include <gx2/display.h>
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
#include <algorithm>
#include <string>

extern const unsigned char _binary_ui_ttf_start[];
extern const unsigned char _binary_ui_ttf_end[];
namespace gx2 { void restoreState(); }

// WHB registers the same foreground callbacks on every GX2 init but its
// shutdown does not remove them. Later HOME events must not run GX2 release
// again after the first callback has already released the foreground buffers.
extern "C" void __real_ProcUIRegisterCallback(ProcUICallbackType, ProcUICallback, void *, uint32_t);
extern "C" void __wrap_ProcUIRegisterCallback(ProcUICallbackType type, ProcUICallback callback,
                                               void *param, uint32_t priority)
{
    struct Entry { ProcUICallbackType type; ProcUICallback callback; void *param; uint32_t priority; };
    static Entry entries[32];
    static unsigned count;
    for (unsigned i = 0; i < count; ++i)
        if (entries[i].type == type && entries[i].callback == callback &&
            entries[i].param == param && entries[i].priority == priority) return;
    if (count < 32) entries[count++] = {type, callback, param, priority};
    __real_ProcUIRegisterCallback(type, callback, param, priority);
}

static bool s_loading;
extern "C" void __real_GX2SetTVEnable(BOOL);
extern "C" void __real_GX2SetDRCEnable(BOOL);
extern "C" void __wrap_GX2SetTVEnable(BOOL enabled) { __real_GX2SetTVEnable(s_loading ? FALSE : enabled); }
extern "C" void __wrap_GX2SetDRCEnable(BOOL enabled) { __real_GX2SetDRCEnable(s_loading ? FALSE : enabled); }
extern "C" void n64_video_begin_loading() {
    s_loading = true;
    __real_GX2SetTVEnable(FALSE);
    __real_GX2SetDRCEnable(FALSE);
}
extern "C" void n64_video_end_loading() { s_loading = false; }

namespace {
GX2ContextState *s_copy_context;
GX2ColorBuffer s_shaped;
bool s_picture_43 = true;
bool s_menu_ready = false;
bool s_menu_drawn = false;
retrou::gx2::Texture s_slot_textures[4] = {-1, -1, -1, -1};
bool copy_context();

void loadSlotTextures(const N64MenuView &view)
{
    using namespace retrou::gx2;
    auto *rgba = static_cast<uint8_t *>(MEMAllocFromDefaultHeapEx(160 * 120 * 4, 64));
    if (!rgba) return;
    for (int slot = 0; slot < 4; ++slot) {
        if (!view.slot_thumbnails[slot]) continue;
        for (int i = 0; i < 160 * 120; ++i) {
            const uint16_t color = view.slot_thumbnails[slot][i];
            rgba[i * 4] = static_cast<uint8_t>(((color >> 11) & 31) * 255 / 31);
            rgba[i * 4 + 1] = static_cast<uint8_t>(((color >> 5) & 63) * 255 / 63);
            rgba[i * 4 + 2] = static_cast<uint8_t>((color & 31) * 255 / 31);
            rgba[i * 4 + 3] = 255;
        }
        s_slot_textures[slot] = createTexture(160, 120, rgba);
    }
    MEMFreeToDefaultHeap(rgba);
}

void captureThumbOnGraphicsCore(void *argument)
{
    auto *out = static_cast<uint16_t *>(argument);
    std::memset(out, 0, 160 * 120 * sizeof(uint16_t));
    GX2ColorBuffer *frame = video_last_frame();
    if (!frame || !copy_context()) return;
    GX2Surface linear = frame->surface;
    linear.use = GX2_SURFACE_USE_TEXTURE;
    linear.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    linear.image = nullptr;
    GX2CalcSurfaceSizeAndAlignment(&linear);
    linear.image = MEMAllocFromDefaultHeapEx(linear.imageSize, linear.alignment);
    if (!linear.image) return;
    GX2SetContextState(s_copy_context);
    GX2CopySurface(&frame->surface, 0, 0, &linear, 0, 0);
    GX2DrawDone();
    DCInvalidateRange(linear.image, linear.imageSize);
    const uint32_t width = linear.width, height = linear.height;
    const uint32_t cropWidth = width < height * 4 / 3 ? width : height * 4 / 3;
    const uint32_t cropHeight = height < width * 3 / 4 ? height : width * 3 / 4;
    const uint32_t left = (width - cropWidth) / 2;
    const uint32_t top = (height - cropHeight) / 2;
    const auto *rgba = static_cast<const uint8_t *>(linear.image);
    for (uint32_t y = 0; y < 120; ++y)
        for (uint32_t x = 0; x < 160; ++x) {
            const uint32_t sx = left + x * cropWidth / 160;
            const uint32_t sy = top + y * cropHeight / 120;
            const uint8_t *p = rgba + (static_cast<size_t>(sy) * linear.pitch + sx) * 4;
            out[y * 160 + x] = static_cast<uint16_t>((p[0] >> 3) << 11 |
                                  (p[1] >> 2) << 5 | (p[2] >> 3));
        }
    MEMFreeToDefaultHeap(linear.image);
    gx2::restoreState();
}

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
                                "Controles", "Salir del juego", "Cerrar aplicación",
                                "A  Aceptar    B  Volver",
                                "Slot 1", "Slot 2", "Slot 3", "Slot 4",
                                "Los controles se configuran en Ajustes"};
        for (const char *label : labels) text::prewarm(label, 20);
        text::prewarm("MENU", 26);
        text::prewarm("Guardar estado", 26);
        text::prewarm("Cargar estado", 26);
        text::prewarm("Controles", 26);
        text::prewarm("Volviendo a la biblioteca...", 26);
        text::prewarm("A  Aceptar    B  Volver", 15);
        text::prewarm("No se pudo guardar el estado", 18);
        s_menu_ready = true;
        loadSlotTextures(view);
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
        drawRoundRect(390, 150, 500, 420, 18, panel);
        text::drawText("MENU", 430, 200, 26, white);
        const char *items[] = {"Continuar", "Guardar estado", "Cargar estado",
                               "Controles", "Salir del juego", "Cerrar aplicación"};
        for (int i = 0; i < 6; ++i)
            text::drawText(items[i], 430, 270 + i * 44, 20,
                           i == view.selected ? accent : muted);
        text::drawText("A  Aceptar    B  Volver", 430, 535, 15, muted);
    } else if (view.page == 4) {
        text::drawText("Volviendo a la biblioteca...", 370, 330, 26, white);
    } else {
        drawRoundRect(290, 110, 700, 500, 18, panel);
        const char *title = view.page == 1 ? "Guardar estado" :
                            view.page == 2 ? "Cargar estado" : "Controles";
        text::drawText(title, 330, 138, 26, white);
        if (view.page == 3) {
            const std::string heading = "Mando " + std::to_string(view.control_port + 1) +
                                        "    IZQ/DER  Cambiar mando";
            text::drawText(heading.c_str(), 330, 190, 18, accent);
            const int first = std::max(0, std::min(11, view.control_row - 3));
            for (int i = 0; i < 7; ++i) {
                const int row = first + i;
                const float y = 240 + i * 43;
                if (row == view.control_row)
                    drawRoundRect(315, y - 7, 650, 36, 8, Color{245, 217, 127, 40});
                text::drawText(view.control_lines[row], 330, y, 17,
                               row == view.control_row ? white : muted);
            }
        } else {
            const char *slots[] = {"Slot 1", "Slot 2", "Slot 3", "Slot 4"};
            for (int i = 0; i < 4; ++i) {
                const float y = 205 + i * 88;
                if (i == view.slot)
                    drawRoundRect(315, y - 10, 650, 78, 10, Color{245, 217, 127, 40});
                if (s_slot_textures[i] >= 0)
                    drawTexture(s_slot_textures[i], 335, y - 5, 90, 68);
                text::drawText(slots[i], 455, y + 18, 20, i == view.slot ? accent : muted);
            }
        }
        text::drawText("A  Aceptar    B  Volver", 330, 570, 15, muted);
        if (view.state_error)
            text::drawText("No se pudo guardar el estado", 330, 540, 18, accent);
    }
    endFrame();
    s_menu_drawn = true;
    gx2::restoreState();
}

void hideMenuOnGraphicsCore(void *)
{
    if (s_menu_ready) {
        GX2DrawDone();
        for (auto &texture : s_slot_textures) {
            if (texture >= 0) retrou::gx2::destroyTexture(texture);
            texture = -1;
        }
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

extern "C" void n64_video_capture_thumb(uint16_t *pixels)
{
    if (pixels) video_call(captureThumbOnGraphicsCore, pixels);
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
    if (s_loading) {
        GX2DrawDone();
        GX2WaitForVsync();
        s_loading = false;
        __real_GX2SetTVEnable(TRUE);
        __real_GX2SetDRCEnable(TRUE);
    }
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
