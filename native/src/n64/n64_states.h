#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Called on the N64 CPU thread after ROM and CPU initialization. */
int n64_states_init(const char *root);
void n64_states_finish(void);

/* Called from the engine's VI callback on that same thread. Slots are 0-3.
 * The work runs at a safe Not64 dispatcher boundary; the last result is
 * available through n64_states_result (0 pending, 1 saved/loaded, <0 error). */
int n64_states_request_save(int slot);
int n64_states_request_load(int slot);
int n64_states_result(void);
int n64_states_load_pending(void);
/* Called only after go() stops; mirrors the pinned engine's load path. */
int n64_states_apply_load(void);

#ifdef __cplusplus
}
#endif
