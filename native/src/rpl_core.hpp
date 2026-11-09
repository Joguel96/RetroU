#pragma once

#include <SDL.h>
#include <coreinit/dynload.h>
#include <libretro.h>

#include <cstdint>
#include <string>
#include <vector>

namespace retrou {

class RplCore {
public:
    RplCore();
    ~RplCore();

    bool load(const std::string &path, SDL_Renderer *tv, SDL_Renderer *drc,
              std::string &error);
    void unload();
    bool loadGame(const std::string &rom_path, std::string &error);
    void runFrame();
    void present(SDL_Renderer *renderer, int width, int height) const;

    bool loaded() const { return module_ != nullptr; }
    const std::string &name() const { return name_; }

private:
    using ApiVersionFn = unsigned (*)();
    using InitFn = void (*)();
    using DeinitFn = void (*)();
    using SetEnvironmentFn = void (*)(retro_environment_t);
    using SetVideoFn = void (*)(retro_video_refresh_t);
    using SetAudioFn = void (*)(retro_audio_sample_t);
    using SetAudioBatchFn = void (*)(retro_audio_sample_batch_t);
    using SetInputPollFn = void (*)(retro_input_poll_t);
    using SetInputStateFn = void (*)(retro_input_state_t);
    using GetSystemInfoFn = void (*)(retro_system_info *);
    using GetAvInfoFn = void (*)(retro_system_av_info *);
    using LoadGameFn = bool (*)(const retro_game_info *);
    using UnloadGameFn = void (*)();
    using RunFn = void (*)();
    using ResetFn = void (*)();
    using SerializeSizeFn = size_t (*)();
    using SerializeFn = bool (*)(void *, size_t);
    using UnserializeFn = bool (*)(const void *, size_t);
    using GetMemoryDataFn = void *(*)(unsigned);
    using GetMemorySizeFn = size_t (*)(unsigned);
    using SetControllerFn = void (*)(unsigned, unsigned);

    OSDynLoad_Module module_ = nullptr;
    SDL_Renderer *tv_ = nullptr;
    SDL_Renderer *drc_ = nullptr;
    SDL_Texture *tv_texture_ = nullptr;
    SDL_Texture *drc_texture_ = nullptr;
    uint32_t pixel_format_ = RETRO_PIXEL_FORMAT_RGB565;
    unsigned texture_width_ = 0;
    unsigned texture_height_ = 0;
    const void *frame_data_ = nullptr;
    unsigned frame_width_ = 0;
    unsigned frame_height_ = 0;
    size_t frame_pitch_ = 0;
    bool game_loaded_ = false;
    bool initialized_ = false;
    std::string name_;
    std::string rom_path_;

    ApiVersionFn api_version_ = nullptr;
    InitFn init_ = nullptr;
    DeinitFn deinit_ = nullptr;
    SetEnvironmentFn set_environment_ = nullptr;
    SetVideoFn set_video_ = nullptr;
    SetAudioFn set_audio_ = nullptr;
    SetAudioBatchFn set_audio_batch_ = nullptr;
    SetInputPollFn set_input_poll_ = nullptr;
    SetInputStateFn set_input_state_ = nullptr;
    GetSystemInfoFn get_system_info_ = nullptr;
    GetAvInfoFn get_av_info_ = nullptr;
    LoadGameFn load_game_ = nullptr;
    UnloadGameFn unload_game_ = nullptr;
    RunFn run_ = nullptr;
    ResetFn reset_ = nullptr;

    static RplCore *instance_;
    static bool environmentCallback(unsigned cmd, void *data);
    static void videoCallback(const void *data, unsigned width, unsigned height, size_t pitch);
    static void audioCallback(int16_t left, int16_t right);
    static size_t audioBatchCallback(const int16_t *data, size_t frames);
    static void inputPollCallback();
    static int16_t inputStateCallback(unsigned port, unsigned device, unsigned index, unsigned id);

    void *resolve(const char *name, std::string &error);
    void updateTexture(SDL_Renderer *renderer, SDL_Texture *&texture);
    void destroyTextures();};

} // namespace retrou
