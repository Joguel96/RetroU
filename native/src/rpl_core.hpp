#pragma once

#include <SDL.h>
#include <libretro.h>

#include "rpl_loader.hpp"

#include <cstdint>
#include <array>
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
    void setSaveDirectory(const std::string &path) { save_directory_ = path; }
    void setController(SDL_GameController *controller) { controllers_[0] = controller; }
    void setControllers(SDL_GameController *first, SDL_GameController *second) {
        controllers_[0] = first;
        controllers_[1] = second;
    }
    void setControlProfile(int profile) { control_profile_ = profile; }
    void setControlMaps(const std::array<std::array<int, 8>, 2> &maps) { control_maps_ = maps; }
    void setVideoOptions(int aspect, int scale, int filter) {
        video_aspect_ = aspect;
        video_scale_ = scale;
        video_filter_ = filter;
    }
    void setAudioOptions(int volume, int latency) {
        audio_volume_ = volume;
        audio_latency_ = latency;
    }
    void startAudio();
    void stopAudio();

    bool loaded() const { return loaded_; }
    const std::string &name() const { return name_; }

#ifdef RETROU_STATIC_CORE
    bool loadStaticCore(SDL_Renderer *tv, SDL_Renderer *drc, std::string &error);
#endif

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

    RplLoader loader_;
    SDL_Renderer *tv_ = nullptr;    SDL_Renderer *drc_ = nullptr;
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
    bool loaded_ = false;
    bool static_test_ = false;
    std::string log_path_ = "/vol/external01/retrou/retrou_rpl.log";
    std::string name_;
    std::string rom_path_;
    std::string save_directory_ = "/vol/external01/retrou/saves/default/save";

    SDL_AudioDeviceID audio_device_ = 0;
    int sample_rate_ = 44100;
    int audio_volume_ = 100;
    int audio_latency_ = 1;
    int video_aspect_ = 0;
    int video_scale_ = 0;
    int video_filter_ = 0;
    SDL_GameController *controller_ = nullptr;
    std::array<SDL_GameController *, 2> controllers_ = {nullptr, nullptr};
    uint16_t input_state_ = 0;
    std::array<uint16_t, 2> input_states_ = {0, 0};
    int control_profile_ = 0;
    std::array<std::array<int, 8>, 2> control_maps_ = {{
        {SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X,
         SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
         SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_START,
         SDL_CONTROLLER_BUTTON_BACK},
        {SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X,
         SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
         SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_START,
         SDL_CONTROLLER_BUTTON_BACK}
    }};

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
    void log(const std::string &message) const;
    void queueAudio(const int16_t *data, size_t frames);
    void pollInput();
    void updateTexture(SDL_Renderer *renderer, SDL_Texture *&texture);
    void destroyTextures();};

} // namespace retrou
