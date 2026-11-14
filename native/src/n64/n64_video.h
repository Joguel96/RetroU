#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Release bridge allocations after the N64 graphics thread stops. */
void n64_video_release();

#ifdef __cplusplus
}
#endif
