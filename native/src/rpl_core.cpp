#include "rpl_core.hpp"
#include "wpad_input.hpp"
#include <padscore/wpad.h>
#include <vpad/input.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <fstream>
#include <iterator>
#include <sys/stat.h>
#include <vector>

namespace retrou {

RplCore *RplCore::instance_ = nullptr;

namespace {

const char *kSystemDirectory = "/vol/external01/retrou/config/system";
constexpr int kAudioSincTaps = 64;
constexpr int kAudioSincPhases = 256;
constexpr int kAudioSincHistory = kAudioSincTaps / 2;

int16_t clampAudioSample(float value)
{
    if (value < -32768.0f) return -32768;
    if (value > 32767.0f) return 32767;
    return static_cast<int16_t>(value);
}

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

    // GBA runs at 59.7275 Hz while the UI clock is near 60 Hz. Pace the
    // producer to the audio device instead of accumulating half a second
    // and periodically cutting the stream during long sessions.
    const uint32_t target_bytes = static_cast<uint32_t>(audio_device_rate_) * 4u / 10u;
    const uint32_t wait_start = SDL_GetTicks();
    while (SDL_GetQueuedAudioSize(audio_device_) > target_bytes &&
           SDL_GetAudioDeviceStatus(audio_device_) == SDL_AUDIO_PLAYING &&
           SDL_GetTicks() - wait_start < 100u) SDL_Delay(1);
    /* Recover if the output device stalled rather than growing without bound. */
    const uint32_t max_bytes = static_cast<uint32_t>(audio_device_rate_) * 4u / 2u;
    if (SDL_GetQueuedAudioSize(audio_device_) > max_bytes) {
        log("Audio queue exceeded 500 ms; dropping stale samples");
        SDL_ClearQueuedAudio(audio_device_);
    }
    if (sample_rate_ != audio_device_rate_) {
        if (audio_resample_kernel_.empty()) return;
        if (audio_resample_buffer_.empty()) {
            audio_resample_buffer_.reserve((frames + kAudioSincTaps) * 2);
            for (int i = 0; i < kAudioSincHistory; ++i) {
                audio_resample_buffer_.push_back(data[0]);
                audio_resample_buffer_.push_back(data[1]);
            }
            audio_resample_position_ = kAudioSincHistory;
        }
        audio_resample_buffer_.insert(audio_resample_buffer_.end(), data, data + frames * 2);
        std::vector<int16_t> converted;
        converted.reserve((frames * static_cast<size_t>(audio_device_rate_) /
                           static_cast<size_t>(sample_rate_) + 2) * 2);
        const double step = static_cast<double>(sample_rate_) / audio_device_rate_;
        const size_t available = audio_resample_buffer_.size() / 2;
        while (static_cast<size_t>(audio_resample_position_) + kAudioSincHistory < available) {
            const size_t center = static_cast<size_t>(audio_resample_position_);
            const double fraction = audio_resample_position_ - center;
            const int phase = static_cast<int>(fraction * kAudioSincPhases);
            const float *kernel = audio_resample_kernel_.data() + phase * kAudioSincTaps;
            float sum[2] = {0.0f, 0.0f};
            for (int tap = 0; tap < kAudioSincTaps; ++tap) {
                const size_t source = (center + tap - (kAudioSincHistory - 1)) * 2;
                sum[0] += audio_resample_buffer_[source] * kernel[tap];
                sum[1] += audio_resample_buffer_[source + 1] * kernel[tap];
            }
            const float gain = audio_volume_ / 100.0f;
            converted.push_back(clampAudioSample(sum[0] * gain));
            converted.push_back(clampAudioSample(sum[1] * gain));
            audio_resample_position_ += step;
        }
        const size_t consumed = static_cast<size_t>(audio_resample_position_) - kAudioSincHistory;
        if (consumed > 0) {
            audio_resample_buffer_.erase(audio_resample_buffer_.begin(),
                                         audio_resample_buffer_.begin() + consumed * 2);
            audio_resample_position_ -= consumed;
        }
        if (!converted.empty() && SDL_QueueAudio(audio_device_, converted.data(),
                           static_cast<uint32_t>(converted.size() * sizeof(int16_t))) != 0)
            log(std::string("SDL_QueueAudio failed: ") + SDL_GetError());
        return;
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

void RplCore::prepareAudioResampler()
{
    audio_resample_kernel_.clear();
    audio_resample_buffer_.clear();
    audio_resample_position_ = 0.0;
    if (sample_rate_ == audio_device_rate_ || audio_device_rate_ <= 0) return;

    // RetroArch resamples core PCM before submitting it to Wii U's 48 kHz
    // output. Use a windowed-sinc low-pass here so mGBA's 65,536 Hz signal
    // can be downsampled without folding frequencies above Nyquist into noise.
    const double pi = 3.14159265358979323846;
    const double cutoff = std::min(1.0, static_cast<double>(audio_device_rate_) / sample_rate_) * 0.90;
    audio_resample_kernel_.resize(kAudioSincPhases * kAudioSincTaps);
    for (int phase = 0; phase < kAudioSincPhases; ++phase) {
        const double fraction = static_cast<double>(phase) / kAudioSincPhases;
        double total = 0.0;
        for (int tap = 0; tap < kAudioSincTaps; ++tap) {
            const double position = tap - (kAudioSincHistory - 1) - fraction;
            const double x = pi * cutoff * position;
            const double sinc = std::abs(x) < 1e-9 ? 1.0 : std::sin(x) / x;
            const double window = 0.42 - 0.5 * std::cos(2.0 * pi * tap / (kAudioSincTaps - 1)) +
                                  0.08 * std::cos(4.0 * pi * tap / (kAudioSincTaps - 1));
            const double coefficient = cutoff * sinc * window;
            audio_resample_kernel_[phase * kAudioSincTaps + tap] = static_cast<float>(coefficient);
            total += coefficient;
        }
        for (int tap = 0; tap < kAudioSincTaps; ++tap)
            audio_resample_kernel_[phase * kAudioSincTaps + tap] /= static_cast<float>(total);
    }
    log("Audio sinc resampler: " + std::to_string(sample_rate_) + " -> " +
        std::to_string(audio_device_rate_) + " Hz, 64 taps");
}

void RplCore::startAudio()
{
    if (!get_av_info_) {
        return;
    }

    retro_system_av_info av{};
    get_av_info_(&av);
    sample_rate_ = av.timing.sample_rate > 0
                       ? static_cast<int>(av.timing.sample_rate + 0.5)
                       : 44100;

    if (audio_device_) {
        SDL_ClearQueuedAudio(audio_device_);
        prepareAudioResampler();
        return;
    }

    if (external_audio_) {
        audio_device_ = external_audio_;
        audio_device_rate_ = external_audio_rate_ > 0 ? external_audio_rate_ : sample_rate_;
        log("Audio reusing external device: " + std::to_string(sample_rate_) +
            " Hz -> " + std::to_string(audio_device_rate_) + " Hz");
        prepareAudioResampler();
        return;
    }

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
    audio_device_rate_ = have.freq;
    prepareAudioResampler();
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

// CTRL-3 (modelo Our64): cada acción con fuentes mapeadas se activa si
// cualquiera de ellas está pulsada. Acción == bit retro (0..11).
static void applyActionMap(const CoreActionMap &m, uint32_t hold, uint16_t &state)
{
    for (int a = 0; a < CoreActionMap::kMaxActions; ++a) {
        if (m.sources[a] != 0 && (m.sources[a] & hold)) {
            state |= static_cast<uint16_t>(1u << a);
        }
    }
}

// Direcciones de los sticks L/R como fuentes pseudo (misma convención de
// signo que Our64: +y = arriba).
static uint32_t stickSourceBits(float lx, float ly, float rx, float ry)
{
    const float dz = 0.5f;
    uint32_t bits = 0;
    if (ly > dz) bits |= kStickSrcUp;
    else if (ly < -dz) bits |= kStickSrcDown;
    else if (lx < -dz) bits |= kStickSrcLeft;
    else if (lx > dz) bits |= kStickSrcRight;
    if (ry > dz) bits |= kStickRSrcUp;
    else if (ry < -dz) bits |= kStickRSrcDown;
    else if (rx < -dz) bits |= kStickRSrcLeft;
    else if (rx > dz) bits |= kStickRSrcRight;
    return bits;
}

void RplCore::pollInput()
{
    if (!input_enabled_) {
        input_states_[0] = 0;
        input_states_[1] = 0;
        input_state_ = 0;
        return;
    }
    // Frontends that push samples (the GX2 app) are consumed here; the rest
    // fall back to polling KPAD from this callback.
    const bool wpadPushed = wpad_samples_used_ != wpad_samples_seq_;
    if (wpadPushed) {
        wpad_samples_used_ = wpad_samples_seq_;
    } else {
        wpad_input_poll();
    }
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
            const WpadSnapshot pad = wpadPushed
                ? wpad_samples_[wpad_ports_[port]]
                : wpad_input_state(wpad_ports_[port]);
            if (pad.connected) {
                const uint32_t hold = (pad.buttons & (pad.type == WPAD_TYPE_PRO ? 0x0003ffffu : 0x0000ffffu)) |
                    stickSourceBits(pad.left_x / 32767.0f, pad.left_y / 32767.0f,
                                    pad.right_x / 32767.0f, pad.right_y / 32767.0f);
                applyActionMap(wpad_map_, hold, state);
            }
        }
        if (vpad_enabled_ && port == vpad_port_ && !controllers_[port] && wpad_ports_[port] < 0) {
            VPADReadError vpadError = VPAD_READ_NO_SAMPLES;
            VPADStatus sample{};
            if (vpad_sample_used_ != vpad_sample_seq_) {
                // Fresh sample pushed by the frontend this frame.
                vpad_sample_used_ = vpad_sample_seq_;
                vpadError = vpad_sample_error_;
                sample = vpad_sample_;
            } else {
                // No frontend sample (e.g. the SDL frontend path): read here.
                VPADRead(VPAD_CHAN_0, &sample, 1, &vpadError);
            }
            last_vpad_error_ = vpadError;
            if (vpadError == VPAD_READ_SUCCESS) {
                const uint32_t hold = (sample.hold & 0x000fffffu) |
                    stickSourceBits(sample.leftStick.x, sample.leftStick.y,
                                    sample.rightStick.x, sample.rightStick.y);
                applyActionMap(vpad_map_, hold, state);
            }
        } else if (port == vpad_port_) {
            last_vpad_error_ = static_cast<VPADReadError>(-100);
        }
        input_states_[port] = state;
        if (port == 0) input_state_ = state;
    }

    // One compact input summary per ROM keeps the runtime log useful.
    if (input_diag_pending_) {
        input_diag_pending_ = false;
        int vpadBound = 0;
        int wpadBound = 0;
        for (int a = 0; a < CoreActionMap::kMaxActions; ++a) {
            vpadBound += vpad_map_.sources[a] != 0 ? 1 : 0;
            wpadBound += wpad_map_.sources[a] != 0 ? 1 : 0;
        }
        char buf[192];
        std::snprintf(buf, sizeof(buf),
                      "input summary: st0=0x%04x st1=0x%04x vpadErr=%d vpadOn=%d wpad=[%d,%d] maps=[%d,%d]",
                      input_states_[0], input_states_[1],
                      static_cast<int>(last_vpad_error_),
                      static_cast<int>(vpad_enabled_), wpad_ports_[0], wpad_ports_[1],
                      vpadBound, wpadBound);
        log(buf);
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
    serialize_size_ = retro_serialize_size;
    serialize_ = retro_serialize;
    unserialize_ = retro_unserialize;
    get_memory_data_ = retro_get_memory_data;
    get_memory_size_ = retro_get_memory_size;
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

    // Default maps are the frontend's responsibility (setVpadActionMap /
    // setWpadActionMap); leave them empty here so unconfigured bindings
    // produce no input.

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
    audio_resample_position_ = 0.0;
    audio_resample_buffer_.clear();
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
    input_diag_pending_ = true;
    loadSram();

    startAudio();

    return true;
}

void RplCore::unloadGame()
{
    log("unloadGame: start");
    if (game_loaded_ && unload_game_) {
        saveSram();
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

std::string RplCore::sramPath() const
{
    std::string name = rom_path_;
    const size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    return save_directory_ + "/" + name + ".srm";
}

bool RplCore::loadSram()
{
    if (!get_memory_data_ || !get_memory_size_) return false;
    const size_t size = get_memory_size_(RETRO_MEMORY_SAVE_RAM);
    if (size == 0) return false;
    void *data = get_memory_data_(RETRO_MEMORY_SAVE_RAM);
    if (!data) return false;
    std::ifstream in(sramPath(), std::ios::binary);
    if (!in) in.open(sramPath() + ".bak", std::ios::binary);
    if (!in) return false;
    in.read(static_cast<char *>(data), static_cast<std::streamsize>(size));
    if (static_cast<size_t>(in.gcount()) != size) return false;
    log("loadSram " + sramPath() + " size=" + std::to_string(size));
    return true;
}

bool RplCore::saveSram()
{
    if (!get_memory_data_ || !get_memory_size_) return false;
    const size_t size = get_memory_size_(RETRO_MEMORY_SAVE_RAM);
    if (size == 0) return false;
    void *data = get_memory_data_(RETRO_MEMORY_SAVE_RAM);
    if (!data) return false;
    const std::string path = sramPath();
    const std::string temporary = path + ".tmp";
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
    out.close();
    if (!out) {
        std::remove(temporary.c_str());
        log("saveSram failed " + path);
        return false;
    }
    const std::string backup = path + ".bak";
    struct stat oldFile{};
    const bool hadOld = stat(path.c_str(), &oldFile) == 0;
    if (hadOld) {
        std::remove(backup.c_str());
        if (std::rename(path.c_str(), backup.c_str()) != 0) {
            std::remove(temporary.c_str());
            log("saveSram backup failed " + path);
            return false;
        }
    }
    if (std::rename(temporary.c_str(), path.c_str()) != 0) {
        if (hadOld) std::rename(backup.c_str(), path.c_str());
        std::remove(temporary.c_str());
        log("saveSram replace failed " + path);
        return false;
    }
    if (hadOld) std::remove(backup.c_str());
    log("saveSram " + path + " size=" + std::to_string(size));
    return true;
}

bool RplCore::saveState(const std::string &path)
{
    saveSram();
    if (!serialize_size_ || !serialize_) return false;
    const size_t size = serialize_size_();
    if (size == 0) return false;
    std::vector<uint8_t> buf(size);
    if (!serialize_(buf.data(), size)) return false;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(reinterpret_cast<const char *>(buf.data()),
              static_cast<std::streamsize>(size));
    log("saveState " + path);
    return true;
}

bool RplCore::loadState(const std::string &path)
{
    if (!unserialize_) return false;
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
    if (buf.empty()) return false;
    const bool ok = unserialize_(buf.data(), buf.size());
    log("loadState " + path + " ok=" + (ok ? "1" : "0"));
    return ok;
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
            saveSram();
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
