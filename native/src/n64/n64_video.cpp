#include "n64_video.h"

#include "report.h"

#include <coreinit/cache.h>
#include <coreinit/memdefaultheap.h>
#include <gx2/context.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/state.h>
#include <gx2/surface.h>
#include <gx2/swap.h>
#include <gx2/texture.h>
#include <cstring>

namespace {
GX2ContextState *s_copy_context;
GX2ColorBuffer s_shaped;
bool s_picture_43 = true;

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
