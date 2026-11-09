#include "rpl_core.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>

namespace retrou {

RplCore *RplCore::instance_ = nullptr;

namespace {

const char *kSystemDirectory = "/vol/external01/retrou/cores/system";
const char *kSaveDirectory = "/vol/external01/retrou/cores/savefiles";

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
                *static_cast<const char **>(data) = kSaveDirectory;
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
    (void)left;
    (void)right;
}

size_t RplCore::audioBatchCallback(const int16_t *data, size_t frames)
{
    (void)data;
    return frames;
}

void RplCore::inputPollCallback() {}

int16_t RplCore::inputStateCallback(unsigned port, unsigned device, unsigned index, unsigned id)
{
    (void)port;
    (void)device;
    (void)index;
    (void)id;
    return 0;
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

#ifdef RETROU_STATIC_TEST
bool RplCore::loadStaticTest(SDL_Renderer *tv, SDL_Renderer *drc, std::string &error)
{
    (void)error;
    unload();
    instance_ = this;
    tv_ = tv;
    drc_ = drc;
    static_test_ = true;
    log_path_ = "/vol/external01/retrou/retrou_rpx.log";
    log("RplCore::loadStaticTest");

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
    return true;
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
    float screen_aspect = static_cast<float>(width) / static_cast<float>(height);
    int draw_w = width;
    int draw_h = height;

    if (frame_aspect > screen_aspect) {
        draw_h = static_cast<int>(width / frame_aspect);
    } else {
        draw_w = static_cast<int>(height * frame_aspect);
    }

    SDL_Rect dest{(width - draw_w) / 2, (height - draw_h) / 2, draw_w, draw_h};
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
        if (initialized_ && deinit_) {
            deinit_();
        }
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
}

} // namespace retrou
