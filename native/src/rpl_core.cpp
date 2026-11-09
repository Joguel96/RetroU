#include "rpl_core.hpp"

#include <cstdio>
#include <cstring>

namespace retrou {

RplCore *RplCore::instance_ = nullptr;

namespace {

const char *kSystemDirectory = "sd:/retrou/cores/system";
const char *kSaveDirectory = "sd:/retrou/cores/savefiles";

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

    if (!loader_.load(path, error)) {
        return false;
    }
    loaded_ = true;

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
        unload();
        return false;
    }

    if (api_version_) {
        api_version_();
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

    init_();
    initialized_ = true;
    return true;
}

bool RplCore::loadGame(const std::string &rom_path, std::string &error)
{
    if (!loaded_ || !load_game_) {
        error = "Core no cargado";
        return false;
    }

    retro_game_info game{};
    rom_path_ = rom_path;
    if (rom_path_.rfind("/vol/external01/", 0) == 0) {
        rom_path_ = "sd:/" + rom_path_.substr(16);
    }
    game.path = rom_path_.c_str();
    game.data = nullptr;
    game.size = 0;
    game.meta = nullptr;

    if (!load_game_(&game)) {
        error = "retro_load_game devolvio false";
        return false;
    }

    game_loaded_ = true;
    return true;
}

void RplCore::runFrame()
{
    if (run_) {
        run_();
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
        loader_.unload();
        loaded_ = false;
    }

    if (instance_ == this) {
        instance_ = nullptr;
    }
    tv_ = nullptr;
    drc_ = nullptr;
    frame_data_ = nullptr;
    name_.clear();
}

} // namespace retrou
