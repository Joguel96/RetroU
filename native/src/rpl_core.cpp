#include "rpl_core.hpp"
#include "wpad_input.hpp"
#include <padscore/wpad.h>
#include <vpad/input.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sys/stat.h>

namespace retrou {

RplCore *RplCore::instance_ = nullptr;

namespace {

const char *kSystemDirectory = "/vol/external01/retrou/config/system";

Uint32 sdlFormatFor(uint32_t format)
{
    switch (format) {
        case RETRO_PIXEL_FORMAT_0RGB1555:
            return SDL_PIXELFORMAT_RGB555;
        case RETRO_PIXEL_FORMAT_XRGB8888:
            return SDL_PIXELFORMAT_ARGB8888;
        case RETRO_PIXEL_FORMAT_RGB565:
        default:
            return SDL_PIXELFORMAT_RGB565;
    }
}

} // namespace

RplCore::RplCore() = default;

RplCore::~RplCore()
{
    unload();
}

void RplCore::log(const std::string &message) const
{
    std::ofstream out(log_path_, std::ios::app);
    if (out) {
        out << message << "\n";
    }
}

bool RplCore::environmentCallback(unsigned cmd, void *data)
{
    switch (cmd) {
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            if (!data) {
                return false;
            }
            uint32_t format = *static_cast<uint32_t *>(data);
            if (format != RETRO_PIXEL_FORMAT_0RGB1555 &&
                format != RETRO_PIXEL_FORMAT_XRGB8888 &&
                format != RETRO_PIXEL_FORMAT_RGB565) {
                return false;
            }
            if (instance_) {
                instance_->pixel_format_ = format;
            }
            return true;
        }
        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
            if (data) {
                *static_cast<bool *>(data) = true;
            }
            return true;
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
            if (data) {
                *static_cast<const char **>(data) = kSystemDirectory;
            }
            return true;
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
            if (data) {
                *static_cast<const char **>(data) =
                    instance_ ? instance_->save_directory_.c_str()
                              : "/vol/external01/retrou/saves/default/save";
            }
            return true;
        case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
            return true;
        case RETRO_ENVIRONMENT_GET_VARIABLE:
            return false;
        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
            if (data) {
                *static_cast<bool *>(data) = false;
            }
            return true;
        case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
            return false;
        case RETRO_ENVIRONMENT_GET_LANGUAGE:
            if (data) {
                *static_cast<unsigned *>(data) = 0;
            }
            return true;
        case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
            if (data) {
                *static_cast<int *>(data) = 3;
            }
            return true;
        case RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE:
            if (data) {
                *static_cast<float *>(data) = 60.0f;
            }
            return true;
        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
        case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
            return true;
        default:
            return false;
    }
}

void RplCore::videoCallback(const void *data, unsigned width, unsigned height, size_t pitch)
{
    if (!instance_ || !data) {
        return;
    }

    instance_->frame_data_ = data;
    instance_->frame_width_ = width;
    instance_->frame_height_ = height;
    instance_->frame_pitch_ = pitch;
    instance_->updateTexture(instance_->tv_, instance_->tv_texture_);
    instance_->updateTexture(instance_->drc_, instance_->drc_texture_);
}

void RplCore::audioCallback(int16_t left, int16_t right)
{
    if (!instance_ || !instance_->audio_device_) {
        return;
    }
    int16_t frame[2] = {left, right};
    instance_->queueAudio(frame, 1);
}

size_t RplCore::audioBatchCallback(const int16_t *data, size_t frames)
{
    if (instance_ && data && frames > 0) {
        instance_->queueAudio(data, frames);
    }
    return frames;
}

void RplCore::queueAudio(const int16_t *data, size_t frames)
{
    if (!audio_device_ || !data || frames == 0) {
        return;
    }

    /* Keep latency bounded if the frontend falls behind. */
    const uint32_t max_bytes = static_cast<uint32_t>(sample_rate_) * 4u * 3u;
    if (SDL_GetQueuedAudioSize(audio_device_) > max_bytes) {
        SDL_ClearQueuedAudio(audio_device_);
    }
    if (audio_volume_ >= 100) {
        SDL_QueueAudio(audio_device_, data,
                       static_cast<uint32_t>(frames * 2 * sizeof(int16_t)));
    } else {
        std::vector<int16_t> scaled(frames * 2);
        for (size_t i = 0; i < scaled.size(); ++i) {
            scaled[i] = static_cast<int16_t>(data[i] * audio_volume_ / 100);
        }
        SDL_QueueAudio(audio_device_, scaled.data(),
                       static_cast<uint32_t>(scaled.size() * sizeof(int16_t)));
    }
}

void RplCore::startAudio()
{
    if (audio_device_ || !get_av_info_) {
        return;
    }

    retro_system_av_info av{};
    get_av_info_(&av);
    sample_rate_ = av.timing.sample_rate > 0
                       ? static_cast<int>(av.timing.sample_rate + 0.5)
                       : 44100;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        log(std::string("SDL audio init failed: ") + SDL_GetError());
        return;
    }

    SDL_AudioSpec want{};
    SDL_AudioSpec have{};
    want.freq = sample_rate_;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = audio_latency_ == 0 ? 512 : audio_latency_ == 2 ? 2048 : 1024;
    want.callback = nullptr;

    audio_device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!audio_device_) {
        log(std::string("SDL_OpenAudioDevice failed: ") + SDL_GetError());
        return;
    }

    log("Audio opened: " + std::to_string(have.freq) + " Hz, " +
        std::to_string(have.channels) + " ch");
    SDL_PauseAudioDevice(audio_device_, 0);
}

void RplCore::stopAudio()
{
    // SDL_CloseAudioDevice hangs on Wii U hardware, so keep the device open and
    // let the OS reclaim it at app exit. Do NOT reset audio_device_ so that
    // startAudio() skips re-opening the device on the next loaded game.
    log("stopAudio: skipped close");
}

void RplCore::inputPollCallback()
{
    if (instance_) {
        instance_->pollInput();
    }
}

void RplCore::pollInput()
{
    wpad_input_poll();
    for (int port = 0; port < 2; ++port) {
        uint16_t state = 0;
        SDL_GameController *controller = controllers_[port];
        if (controller) {
            auto pressed = [controller](SDL_GameControllerButton button) {
                return SDL_GameControllerGetButton(controller, button) != 0;
            };
            const auto &map = control_maps_[port];
            if (pressed(static_cast<SDL_GameControllerButton>(map[0]))) state |= 1u << RETRO_DEVICE_ID_JOYPAD_A;
            if (pressed(static_cast<SDL_GameControllerButton>(map[1]))) state |= 1u << RETRO_DEVICE_ID_JOYPAD_B;
            if (pressed(static_cast<SDL_GameControllerButton>(map[2]))) state |= 1u << RETRO_DEVICE_ID_JOYPAD_X;
            if (pressed(static_cast<SDL_GameControllerButton>(map[3]))) state |= 1u << RETRO_DEVICE_ID_JOYPAD_Y;
            if (pressed(static_cast<SDL_GameControllerButton>(map[6]))) state |= 1u << RETRO_DEVICE_ID_JOYPAD_START;
            if (pressed(static_cast<SDL_GameControllerButton>(map[7]))) state |= 1u << RETRO_DEVICE_ID_JOYPAD_SELECT;
            if (pressed(static_cast<SDL_GameControllerButton>(map[4]))) state |= 1u << RETRO_DEVICE_ID_JOYPAD_L;
            if (pressed(static_cast<SDL_GameControllerButton>(map[5]))) state |= 1u << RETRO_DEVICE_ID_JOYPAD_R;
            if (pressed(SDL_CONTROLLER_BUTTON_DPAD_UP)) state |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
            if (pressed(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) state |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
            if (pressed(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) state |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
            if (pressed(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) state |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;

            const int deadzone = 12000;
            int lx = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
            int ly = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
            if (lx < -deadzone) state |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
            if (lx > deadzone) state |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
            if (ly < -deadzone) state |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
            if (ly > deadzone) state |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
        }
        if (wpad_ports_[port] >= 0) {
            WpadSnapshot pad = wpad_input_state(wpad_ports_[port]);
            if (pad.connected) {
                if (pad.buttons & WPAD_PRO_BUTTON_A) state |= 1u << RETRO_DEVICE_ID_JOYPAD_A;
                if (pad.buttons & WPAD_PRO_BUTTON_B) state |= 1u << RETRO_DEVICE_ID_JOYPAD_B;
                if (pad.buttons & WPAD_PRO_BUTTON_X) state |= 1u << RETRO_DEVICE_ID_JOYPAD_X;
                if (pad.buttons & WPAD_PRO_BUTTON_Y) state |= 1u << RETRO_DEVICE_ID_JOYPAD_Y;
                if (pad.buttons & WPAD_PRO_BUTTON_L) state |= 1u << RETRO_DEVICE_ID_JOYPAD_L;
                if (pad.buttons & WPAD_PRO_BUTTON_R) state |= 1u << RETRO_DEVICE_ID_JOYPAD_R;
                if (pad.buttons & WPAD_PRO_BUTTON_PLUS) state |= 1u << RETRO_DEVICE_ID_JOYPAD_START;
                if (pad.buttons & WPAD_PRO_BUTTON_MINUS) state |= 1u << RETRO_DEVICE_ID_JOYPAD_SELECT;
                if (pad.buttons & WPAD_PRO_BUTTON_UP) state |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
                if (pad.buttons & WPAD_PRO_BUTTON_DOWN) state |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
                if (pad.buttons & WPAD_PRO_BUTTON_LEFT) state |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
                if (pad.buttons & WPAD_PRO_BUTTON_RIGHT) state |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
            }
        }
        if (port == 0 && vpad_enabled_ && !controllers_[0] && wpad_ports_[0] < 0) {
            VPADStatus vpad{};
            VPADReadError vpadError = VPAD_READ_NO_SAMPLES;
            VPADRead(VPAD_CHAN_0, &vpad, 1, &vpadError);
            if (vpadError == VPAD_READ_SUCCESS) {
                if (vpad.hold & VPAD_BUTTON_A) state |= 1u << RETRO_DEVICE_ID_JOYPAD_A;
                if (vpad.hold & VPAD_BUTTON_B) state |= 1u << RETRO_DEVICE_ID_JOYPAD_B;
                if (vpad.hold & VPAD_BUTTON_X) state |= 1u << RETRO_DEVICE_ID_JOYPAD_X;
                if (vpad.hold & VPAD_BUTTON_Y) state |= 1u << RETRO_DEVICE_ID_JOYPAD_Y;
                if (vpad.hold & VPAD_BUTTON_L) state |= 1u << RETRO_DEVICE_ID_JOYPAD_L;
                if (vpad.hold & VPAD_BUTTON_R) state |= 1u << RETRO_DEVICE_ID_JOYPAD_R;
                if (vpad.hold & VPAD_BUTTON_PLUS) state |= 1u << RETRO_DEVICE_ID_JOYPAD_START;
                if (vpad.hold & VPAD_BUTTON_MINUS) state |= 1u << RETRO_DEVICE_ID_JOYPAD_SELECT;
                if (vpad.hold & VPAD_BUTTON_UP) state |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
                if (vpad.hold & VPAD_BUTTON_DOWN) state |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
                if (vpad.hold & VPAD_BUTTON_LEFT) state |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
                if (vpad.hold & VPAD_BUTTON_RIGHT) state |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
            }
        }
        input_states_[port] = state;
        if (port == 0) input_state_ = state;
    }
}

int16_t RplCore::inputStateCallback(unsigned port, unsigned device, unsigned index, unsigned id)
{
    (void)index;
    if (!instance_ || port >= 2) {
        return 0;
    }
    if (device != RETRO_DEVICE_JOYPAD) {
        return 0;
    }
    if (id == RETRO_DEVICE_ID_JOYPAD_MASK) {
        return static_cast<int16_t>(instance_->input_states_[port]);
    }
    if (id >= 16) {
        return 0;
    }
    return (instance_->input_states_[port] & (1u << id)) ? 1 : 0;
}

void *RplCore::resolve(const char *symbol, std::string &error)
{
    void *address = loader_.findExport(symbol);
    if (!address) {
        error = std::string("Falta export ") + symbol;
        return nullptr;
    }
    return address;
}

bool RplCore::load(const std::string &path, SDL_Renderer *tv, SDL_Renderer *drc,
                   std::string &error)
{
    unload();
    instance_ = this;
    tv_ = tv;
    drc_ = drc;
    static_test_ = false;
    log_path_ = "/vol/external01/retrou/retrou_rpl.log";
    log("RplCore::load " + path);

    if (!loader_.load(path, error)) {
        log("RplCore loader failed: " + error);
        return false;
    }
    loaded_ = true;
    log("RplCore loader ok");

    api_version_ = reinterpret_cast<ApiVersionFn>(resolve("retro_api_version", error));
    init_ = reinterpret_cast<InitFn>(resolve("retro_init", error));
    deinit_ = reinterpret_cast<DeinitFn>(resolve("retro_deinit", error));
    set_environment_ = reinterpret_cast<SetEnvironmentFn>(resolve("retro_set_environment", error));
    set_video_ = reinterpret_cast<SetVideoFn>(resolve("retro_set_video_refresh", error));
    set_audio_ = reinterpret_cast<SetAudioFn>(resolve("retro_set_audio_sample", error));
    set_audio_batch_ = reinterpret_cast<SetAudioBatchFn>(resolve("retro_set_audio_sample_batch", error));
    set_input_poll_ = reinterpret_cast<SetInputPollFn>(resolve("retro_set_input_poll", error));
    set_input_state_ = reinterpret_cast<SetInputStateFn>(resolve("retro_set_input_state", error));
    get_system_info_ = reinterpret_cast<GetSystemInfoFn>(resolve("retro_get_system_info", error));
    get_av_info_ = reinterpret_cast<GetAvInfoFn>(resolve("retro_get_system_av_info", error));
    load_game_ = reinterpret_cast<LoadGameFn>(resolve("retro_load_game", error));
    unload_game_ = reinterpret_cast<UnloadGameFn>(resolve("retro_unload_game", error));
    run_ = reinterpret_cast<RunFn>(resolve("retro_run", error));
    reset_ = reinterpret_cast<ResetFn>(resolve("retro_reset", error));

    if (!error.empty()) {
        log("RplCore resolve failed: " + error);
        unload();
        return false;
    }
    log("RplCore exports ok");

    if (api_version_) {
        log("retro_api_version ptr=" +
                 std::to_string(reinterpret_cast<uintptr_t>(api_version_)));
        log("before retro_api_version");
        unsigned version = api_version_();
        log("after retro_api_version " + std::to_string(version));
    }

    set_environment_(environmentCallback);
    set_video_(videoCallback);
    set_audio_(audioCallback);
    set_audio_batch_(audioBatchCallback);
    set_input_poll_(inputPollCallback);
    set_input_state_(inputStateCallback);

    retro_system_info info{};
    get_system_info_(&info);
    name_ = info.library_name ? info.library_name : "core";

    log("RplCore before retro_init");
    init_();
    initialized_ = true;
    log("RplCore after retro_init");
    return true;
}

#ifdef RETROU_STATIC_CORE
bool RplCore::loadStaticCore(SDL_Renderer *tv, SDL_Renderer *drc, std::string &error)
{
    (void)error;
    unload();
    instance_ = this;
    tv_ = tv;
    drc_ = drc;
    static_test_ = true;
    log_path_ = "/vol/external01/retrou/retrou_rpx.log";
    mkdir("/vol/external01/retrou/config/system", 0777);
    mkdir("/vol/external01/retrou/saves/default/save", 0777);
    log("RplCore::loadStaticCore");

    api_version_ = retro_api_version;
    init_ = retro_init;
    deinit_ = retro_deinit;
    set_environment_ = retro_set_environment;
    set_video_ = retro_set_video_refresh;
    set_audio_ = retro_set_audio_sample;
    set_audio_batch_ = retro_set_audio_sample_batch;
    set_input_poll_ = retro_set_input_poll;
    set_input_state_ = retro_set_input_state;
    get_system_info_ = retro_get_system_info;
    get_av_info_ = retro_get_system_av_info;
    load_game_ = retro_load_game;
    unload_game_ = retro_unload_game;
    run_ = retro_run;
    reset_ = retro_reset;
    loaded_ = true;

    set_environment_(environmentCallback);
    set_video_(videoCallback);
    set_audio_(audioCallback);
    set_audio_batch_(audioBatchCallback);
    set_input_poll_(inputPollCallback);
    set_input_state_(inputStateCallback);

    retro_system_info info{};
    get_system_info_(&info);
    name_ = info.library_name ? info.library_name : "static test";

    log("RplCore before retro_init (static)");
    init_();
    initialized_ = true;
    log("RplCore after retro_init (static)");
    return true;
}
#endif

bool RplCore::loadGame(const std::string &rom_path, std::string &error)
{
    if (!loaded_ || !load_game_) {
        error = "Core no cargado";
        return false;
    }
    retro_game_info game{};
    rom_path_ = rom_path;
    log("RplCore loadGame " + rom_path_);
    game.path = rom_path_.c_str();
    game.data = nullptr;
    game.size = 0;
    game.meta = nullptr;

    if (!load_game_(&game)) {
        error = "retro_load_game devolvio false";
        log("retro_load_game failed");
        return false;
    }

    log("retro_load_game ok");
    game_loaded_ = true;

    startAudio();

    return true;
}

void RplCore::unloadGame()
{
    log("unloadGame: start");
    if (game_loaded_ && unload_game_) {
        unload_game_();
    }
    log("unloadGame: after unload_game_");
    game_loaded_ = false;
    frame_data_ = nullptr;
    frame_width_ = 0;
    frame_height_ = 0;
    frame_pitch_ = 0;
    log("unloadGame: done");
}

void RplCore::runFrame()
{
    static int frame_count = 0;
    if (frame_count < 3) {
        log("retro_run before " + std::to_string(frame_count));
    }
    if (run_) {
        run_();
    }
    if (frame_count < 3) {
        log("retro_run after " + std::to_string(frame_count));
        ++frame_count;
    }
}

void RplCore::updateTexture(SDL_Renderer *renderer, SDL_Texture *&texture)
{
    if (!renderer || !frame_data_ || frame_width_ == 0 || frame_height_ == 0) {
        return;
    }

    if (!texture || texture_width_ != frame_width_ || texture_height_ != frame_height_) {
        if (texture) {
            SDL_DestroyTexture(texture);
        }
        texture = SDL_CreateTexture(renderer, sdlFormatFor(pixel_format_),
                                    SDL_TEXTUREACCESS_STREAMING,
                                    static_cast<int>(frame_width_),
                                    static_cast<int>(frame_height_));
        texture_width_ = frame_width_;
        texture_height_ = frame_height_;
        if (texture) {
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
        }
    }

    if (texture) {
        SDL_UpdateTexture(texture, nullptr, frame_data_, static_cast<int>(frame_pitch_));
    }
}

void RplCore::present(SDL_Renderer *renderer, int width, int height) const
{
    SDL_Texture *texture = renderer == tv_ ? tv_texture_ : drc_texture_;
    if (!renderer || !texture || frame_width_ == 0 || frame_height_ == 0) {
        return;
    }

    float frame_aspect = static_cast<float>(frame_width_) / static_cast<float>(frame_height_);
    if (video_aspect_ == 1) frame_aspect = 4.0f / 3.0f;
    else if (video_aspect_ == 2) frame_aspect = 16.0f / 9.0f;
    float screen_aspect = static_cast<float>(width) / static_cast<float>(height);
    int draw_w = width;
    int draw_h = height;

    if (video_scale_ == 1) {
        int sx = static_cast<int>(width) / static_cast<int>(frame_width_);
        int sy = static_cast<int>(height) / static_cast<int>(frame_height_);
        int scale = std::max(1, std::min(sx, sy));
        draw_w = static_cast<int>(frame_width_) * scale;
        draw_h = static_cast<int>(frame_height_) * scale;
    } else if (frame_aspect > screen_aspect) {
        draw_h = static_cast<int>(width / frame_aspect);
    } else {
        draw_w = static_cast<int>(height * frame_aspect);
    }

    SDL_Rect dest{(width - draw_w) / 2, (height - draw_h) / 2, draw_w, draw_h};
    SDL_SetTextureScaleMode(texture, video_filter_ ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
    SDL_RenderCopy(renderer, texture, nullptr, &dest);
}

void RplCore::destroyTextures()
{
    if (tv_texture_) {
        SDL_DestroyTexture(tv_texture_);
        tv_texture_ = nullptr;
    }
    if (drc_texture_) {
        SDL_DestroyTexture(drc_texture_);
        drc_texture_ = nullptr;
    }
    texture_width_ = 0;
    texture_height_ = 0;
}

void RplCore::unload()
{
    log("RplCore::unload");
    if (loaded_) {
        if (game_loaded_ && unload_game_) {
            unload_game_();
        }
        game_loaded_ = false;
        stopAudio();
        log("unload: after stopAudio");
        if (initialized_ && deinit_) {
            deinit_();
        }
        log("unload: after deinit");
        initialized_ = false;
        destroyTextures();
        if (!static_test_) {
            loader_.unload();
        }
        loaded_ = false;
    }
    static_test_ = false;

    if (instance_ == this) {
        instance_ = nullptr;
    }
    tv_ = nullptr;
    drc_ = nullptr;
    frame_data_ = nullptr;
    name_.clear();
    log("unload: end");
}

} // namespace retrou
