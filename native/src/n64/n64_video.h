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
    int output_mode;
    uint8_t background_top[4];
    uint8_t background[4];
    uint8_t accent[4];
} N64MenuView;

/* Synchronous graphics-thread calls while the N64 CPU is paused at a VI. */
int n64_video_menu_show(const N64MenuView *view);
void n64_video_menu_hide(void);

#ifdef __cplusplus
}
#endif
