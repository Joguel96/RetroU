#pragma once

#include <SDL.h>
#include <libretro.h>

#include "rpl_loader.hpp"

#include <cstdint>
#include <array>
#include <string>
#include <vector>

#include "wpad_input.hpp"
#include <vpad/input.h>

namespace retrou {

// CTRL-3 (modelo Our64): cada acción del core (ids retro 0..11) apunta a UNA
// O VARIAS fuentes físicas (máscara del dispositivo). Una fuente puede estar
// en varias acciones (Y Add comparte); el editor reasigna al reemplazar.
// Las direcciones de los sticks L/R son fuentes pseudo (kStickSrc*), así que
// cada stick puede actuar como D-Pad en las direcciones que haya mapeadas.
struct CoreActionMap {
    static constexpr int kMaxActions = 12;
    std::array<uint32_t, kMaxActions> sources{};
};

// Pseudo-masks for the stick directions. Same values for the VPAD and WPAD
// mask spaces; the real button masks of both stay below 0x01000000
// (VPAD tops out at 0x00040000 STICK_L, WPAD at 0x00020000).
static constexpr uint32_t kStickSrcUp = 0x01000000u;
static constexpr uint32_t kStickSrcDown = 0x02000000u;
static constexpr uint32_t kStickSrcLeft = 0x04000000u;
static constexpr uint32_t kStickSrcRight = 0x08000000u;
static constexpr uint32_t kStickRSrcUp = 0x10000000u;
static constexpr uint32_t kStickRSrcDown = 0x20000000u;
static constexpr uint32_t kStickRSrcLeft = 0x40000000u;
static constexpr uint32_t kStickRSrcRight = 0x80000000u;

class RplCore {
public:
    RplCore();
    ~RplCore();

    bool load(const std::string &path, SDL_Renderer *tv, SDL_Renderer *drc,
              std::string &error);
    void unload();
    bool loadGame(const std::string &rom_path, std::string &error);
    // Unloads only the running game (keeps the core initialized and audio open).
    // Used when returning to the library so we don't deinit/reinit the core.
    void unloadGame();
    bool loadSram();
    bool saveSram();
    bool saveState(const std::string &path);
    bool loadState(const std::string &path);
    void runFrame();
    void present(SDL_Renderer *renderer, int width, int height) const;
    // Frame access for a GX2 frontend: the core exposes the last rendered
    // frame so the renderer can upload it as a texture (see uploadFrame).
    bool hasFrame() const { return frame_data_ != nullptr && frame_width_ > 0; }
    const void *frameData() const { return frame_data_; }
    uint32_t frameWidth() const { return frame_width_; }
    uint32_t frameHeight() const { return frame_height_; }
    size_t framePitch() const { return frame_pitch_; }
    uint32_t pixelFormat() const { return pixel_format_; }
    void setSaveDirectory(const std::string &path) { save_directory_ = path; }
    void setController(SDL_GameController *controller) { controllers_[0] = controller; }
    void setControllers(SDL_GameController *first, SDL_GameController *second) {
        controllers_[0] = first;
        controllers_[1] = second;
    }
    void setControlProfile(int profile) { control_profile_ = profile; }
    void setControlMaps(const std::array<std::array<int, 8>, 2> &maps) { control_maps_ = maps; }
    void setWpadPorts(int first, int second) { wpad_ports_[0] = first; wpad_ports_[1] = second; }
    void setVpadEnabled(bool enabled) { vpad_enabled_ = enabled; }
    // Which core port (0/1) the GamePad feeds; -1 = GamePad not assigned.
    void setVpadPort(int port) { vpad_port_ = port; }
    // Action maps (CTRL-3, modelo Our64): acción -> máscaras físicas.
    void setVpadActionMap(const CoreActionMap &map) { vpad_map_ = map; }
    void setWpadActionMap(const CoreActionMap &map) { wpad_map_ = map; }
    void setInputEnabled(bool on) { input_enabled_ = on; }
    // The frontend reads VPAD once per frame; the core must reuse that
    // sample, because a second VPADRead in the same frame returns
    // NO_SAMPLES (the queue only refills between frames). The frontend
    // pushes it here right after its own read.
    void pushVpadSample(const VPADStatus &vpad, VPADReadError error) {
        vpad_sample_ = vpad;
        vpad_sample_error_ = error;
        ++vpad_sample_seq_;
    }
    // Same idea for the KPAD/WPAD controllers: the frontend polls once per
    // frame and pushes all four channels; the core reuses them.
    void pushWpadSamples(const std::array<WpadSnapshot, 4> &states) {
        wpad_samples_ = states;
        ++wpad_samples_seq_;
    }
    void setExternalAudioDevice(SDL_AudioDeviceID device, int rate) {
        external_audio_ = device;
        external_audio_rate_ = rate;
    }
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
    SerializeSizeFn serialize_size_ = nullptr;
    SerializeFn serialize_ = nullptr;
    UnserializeFn unserialize_ = nullptr;
    GetMemoryDataFn get_memory_data_ = nullptr;
    GetMemorySizeFn get_memory_size_ = nullptr;
    std::string sramPath() const;
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
    int audio_device_rate_ = 44100;
    int external_audio_rate_ = 0;
    double audio_resample_phase_ = 0.0;
    int16_t audio_previous_[2] = {0, 0};
    bool audio_previous_valid_ = false;
    int audio_volume_ = 100;
    int audio_latency_ = 1;
    int video_aspect_ = 0;
    int video_scale_ = 0;
    int video_filter_ = 0;
    int wpad_ports_[2] = {-1, -1};
    bool vpad_enabled_ = false;
    int vpad_port_ = 0;
    bool input_enabled_ = true;
    VPADReadError last_vpad_error_ = VPAD_READ_NO_SAMPLES;
    VPADStatus vpad_sample_{};
    VPADReadError vpad_sample_error_ = VPAD_READ_NO_SAMPLES;
    int vpad_sample_seq_ = 0;   // bumped by every pushVpadSample()
    int vpad_sample_used_ = 0;  // seq consumed by the last poll
    std::array<WpadSnapshot, 4> wpad_samples_{};
    int wpad_samples_seq_ = 0;
    int wpad_samples_used_ = 0;
    CoreActionMap vpad_map_;
    CoreActionMap wpad_map_;
    SDL_AudioDeviceID external_audio_ = 0;
    SDL_GameController *controller_ = nullptr;
    std::array<SDL_GameController *, 2> controllers_ = {nullptr, nullptr};
    uint16_t input_state_ = 0;
    std::array<uint16_t, 2> input_states_ = {0, 0};
    bool input_diag_pending_ = true;
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
