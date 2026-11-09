#include <stdint.h>
#include <string.h>

#include "libretro.h"

static retro_video_refresh_t video_cb;
static retro_environment_t environment_cb;
static uint16_t frame[256 * 240];

extern "C" void __rpl_start(void) {}

extern "C" unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}

extern "C" void retro_set_environment(retro_environment_t cb) { environment_cb = cb; }
extern "C" void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
extern "C" void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
extern "C" void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { (void)cb; }
extern "C" void retro_set_input_poll(retro_input_poll_t cb) { (void)cb; }
extern "C" void retro_set_input_state(retro_input_state_t cb) { (void)cb; }

extern "C" void retro_init(void)
{
    if (environment_cb) {
        uint32_t format = RETRO_PIXEL_FORMAT_RGB565;
        environment_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format);
    }
}

extern "C" void retro_deinit(void) {}

extern "C" void retro_get_system_info(struct retro_system_info *info)
{
    memset(info, 0, sizeof(*info));
    info->library_name = "RetroU Test";
    info->library_version = "1.0";
    info->valid_extensions = "nes";
    info->need_fullpath = false;
    info->block_extract = false;
}

extern "C" void retro_get_system_av_info(struct retro_system_av_info *info)
{
    memset(info, 0, sizeof(*info));
    info->geometry.base_width = 256;
    info->geometry.base_height = 240;
    info->geometry.max_width = 256;
    info->geometry.max_height = 240;
    info->geometry.aspect_ratio = 0.0f;
    info->timing.fps = 60.0;
    info->timing.sample_rate = 48000.0;
}

extern "C" void retro_set_controller_port_device(unsigned port, unsigned device)
{
    (void)port;
    (void)device;
}

extern "C" void retro_reset(void) {}

extern "C" void retro_run(void)
{
    static unsigned tick = 0;
    uint16_t color = static_cast<uint16_t>(0x8000 | ((tick & 0x1F) << 10) |
                                            (((tick * 3) & 0x1F) << 5) |
                                            ((tick * 7) & 0x1F));
    for (int i = 0; i < 256 * 240; ++i) {
        frame[i] = color;
    }
    ++tick;

    if (video_cb) {
        video_cb(frame, 256, 240, 256 * sizeof(uint16_t));
    }
}

extern "C" size_t retro_serialize_size(void) { return 0; }
extern "C" bool retro_serialize(void *data, size_t size) { (void)data; (void)size; return false; }
extern "C" bool retro_unserialize(const void *data, size_t size) { (void)data; (void)size; return false; }

extern "C" void retro_cheat_reset(void) {}
extern "C" void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
    (void)index;
    (void)enabled;
    (void)code;
}

extern "C" bool retro_load_game(const struct retro_game_info *game)
{
    (void)game;
    return true;
}

extern "C" bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
    (void)type;
    (void)info;
    (void)num;
    return false;
}

extern "C" void retro_unload_game(void) {}
extern "C" unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
extern "C" void *retro_get_memory_data(unsigned id) { (void)id; return nullptr; }
extern "C" size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
