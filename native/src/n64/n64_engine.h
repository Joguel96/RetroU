#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* Called on the N64 CPU thread at every VI. Return nonzero to end play. */
    int (*on_vi)(void *user, unsigned vi, uint32_t ports[4]);
    /* Rumble changes can arrive on the CPU thread. */
    void (*on_rumble)(void *user, int port, int enabled);
    void *user;
} N64EngineHost;

/* Run on a worker pinned to OSGetCodegenCore(), after RetroU releases GX2.
 * This call owns GLideN64's GX2 thread until it returns. root must already
 * exist and stays under sd:/retrou/n64. Returns 0 after a normal stop. */
int n64_engine_play(const char *rom_path, const char *root, const N64EngineHost *host);
/* Safe to call from RetroU's UI/ProcUI thread. */
void n64_engine_request_stop(void);

#ifdef __cplusplus
}
#endif
