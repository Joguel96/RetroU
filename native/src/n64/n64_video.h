#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Release bridge allocations after the N64 graphics thread stops. */
void n64_video_release();

typedef struct {
    int selected;
    int page; /* 0: main, 1: save, 2: load, 3: controls */
    int slot;
    int state_error;
    int control_port;
    int control_row;
    char control_lines[18][96];
    const uint16_t *slot_thumbnails[4];
    int output_mode;
    uint8_t background_top[4];
    uint8_t background[4];
    uint8_t accent[4];
} N64MenuView;

/* Synchronous graphics-thread calls while the N64 CPU is paused at a VI. */
int n64_video_menu_show(const N64MenuView *view);
void n64_video_menu_hide(void);
void n64_video_capture_thumb(uint16_t *pixels); /* 160 x 120 RGB565 */

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
void n64_video_begin_loading(void);
void n64_video_end_loading(void);
#ifdef __cplusplus
}
#endif
