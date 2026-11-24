#pragma once

#include "n64_session.hpp"
#include "rpl_core.hpp" // RetroU's physical action maps and frontend types

#include <padscore/wpad.h>

#include <memory>
#include <string>

// Adapter for the shared RetroU GX2 library screen. N64 renders directly on
// GLideN64's GX2 thread, so frame access and runFrame are intentionally empty.
// The frontend releases its own GX2 resources before calling startGame().
class N64FrontendCore {
public:
    N64FrontendCore() : session_(new N64Session) {}

    bool loadStaticCore(SDL_Renderer *, SDL_Renderer *, std::string &error)
    {
        error.clear();
        return true;
    }
    bool loadGame(const std::string &path, std::string &error)
    {
        if (path.empty() || running_) {
            error = "No se puede abrir la ROM N64";
            return false;
        }
        romPath_ = path;
        error.clear();
        return true;
    }
    bool startGame(const std::string &root)
    {
        if (romPath_.empty() || running_) return false;
        for (int port = 0; port < 4; ++port) session_->assign(port, -1);
        for (int slot = 0; slot < 4; ++slot)
            session_->setChannel(slot, channels_[slot]);
        for (int port = 0; port < 4; ++port)
            session_->assign(port, devices_[port]);
        session_->setMap(INPUT_GAMEPAD, translatedMap(vpadActionMap_, false, INPUT_GAMEPAD));
        session_->setMap(INPUT_PRO, translatedMap(proActionMap_, true, INPUT_PRO));
        session_->setMap(INPUT_CLASSIC, translatedMap(proActionMap_, true, INPUT_CLASSIC));
        running_ = session_->start(romPath_, root);
        return running_;
    }
    void requestStop() { if (running_) session_->requestStop(); }
    int takeMenuRequest() { return session_->takeMenuRequest(); }
    uint32_t takeMenuButtons() { return session_->takeMenuButtons(); }
    bool menuPaused() const { return session_->menuPaused(); }
    void resumeMenu() { session_->resumeMenu(); }
    bool requestSave(int slot, const uint16_t *thumbnail) {
        return session_->requestSave(slot, thumbnail);
    }
    int stateResult() const { return session_->stateResult(); }
    bool readStateThumbnail(int slot, uint16_t *pixels) const {
        return session_->readStateThumbnail(slot, pixels);
    }
    bool requestLoad(int slot) { return session_->requestLoad(slot); }
    unsigned startupStage() const { return session_->startupStage(); }
    int resumeCount() const { return session_->resumeCount(); }
    bool finished() const { return running_ && session_->finished(); }
    int join()
    {
        if (!running_) return -1;
        const int result = session_->join();
        running_ = false;
        return result;
    }
    void unloadGame()
    {
        if (running_) { session_->requestStop(); join(); }
        romPath_.clear();
    }
    bool saveState(const std::string &) { return false; }
    bool loadState(const std::string &) { return false; }
    void runFrame() {}
    bool hasFrame() const { return false; }
    const void *frameData() const { return nullptr; }
    uint32_t frameWidth() const { return 0; }
    uint32_t frameHeight() const { return 0; }
    size_t framePitch() const { return 0; }
    uint32_t pixelFormat() const { return RETRO_PIXEL_FORMAT_RGB565; }

    void pushVpadSample(const VPADStatus &, VPADReadError) {}
    void pushWpadSamples(const std::array<WpadSnapshot, 4> &) {}
    void setSaveDirectory(const std::string &) {}
    void setControllers(SDL_GameController *, SDL_GameController *) {}
    void setVpadEnabled(bool) {}
    void setVpadPort(int port) { vpadPort_ = port; }
    void setWpadPorts(int first, int second)
    {
        wpadPorts_[0] = first;
        wpadPorts_[1] = second;
    }
    void setPortDevices(const std::array<int, 4> &devices,
                        const std::array<int, 4> &channels)
    {
        devices_ = devices;
        channels_ = channels;
    }
    void setVpadActionMap(const retrou::CoreActionMap &map) { vpadActionMap_ = map; }
    void setWpadActionMap(const retrou::CoreActionMap &map) { proActionMap_ = map; }
    void setInputEnabled(bool) {}
    void setVideoOptions(int, int, int) {}
    void setExternalAudioDevice(SDL_AudioDeviceID, int) {}
    void setAudioOptions(int, int) {}

private:
    static input_map translatedMap(const retrou::CoreActionMap &actions, bool pro, int type)
    {
        input_map result{};
        input_default_map(type, &result);
        struct SourceButton { uint32_t vpad, wpad; int source; };
        static const SourceButton buttons[] = {
            {VPAD_BUTTON_A, WPAD_PRO_BUTTON_A, SRC_A},
            {VPAD_BUTTON_B, WPAD_PRO_BUTTON_B, SRC_B},
            {VPAD_BUTTON_X, WPAD_PRO_BUTTON_X, SRC_X},
            {VPAD_BUTTON_Y, WPAD_PRO_BUTTON_Y, SRC_Y},
            {VPAD_BUTTON_L, WPAD_PRO_BUTTON_L, SRC_L},
            {VPAD_BUTTON_R, WPAD_PRO_BUTTON_R, SRC_R},
            {VPAD_BUTTON_ZL, WPAD_PRO_BUTTON_ZL, SRC_ZL},
            {VPAD_BUTTON_ZR, WPAD_PRO_BUTTON_ZR, SRC_ZR},
            {VPAD_BUTTON_PLUS, WPAD_PRO_BUTTON_PLUS, SRC_PLUS},
            {VPAD_BUTTON_MINUS, WPAD_PRO_BUTTON_MINUS, SRC_MINUS},
            {VPAD_BUTTON_UP, WPAD_PRO_BUTTON_UP, SRC_UP},
            {VPAD_BUTTON_DOWN, WPAD_PRO_BUTTON_DOWN, SRC_DOWN},
            {VPAD_BUTTON_LEFT, WPAD_PRO_BUTTON_LEFT, SRC_LEFT},
            {VPAD_BUTTON_RIGHT, WPAD_PRO_BUTTON_RIGHT, SRC_RIGHT},
            {VPAD_BUTTON_STICK_L, WPAD_PRO_BUTTON_STICK_L, SRC_LSTICK_PRESS},
            {VPAD_BUTTON_STICK_R, WPAD_PRO_BUTTON_STICK_R, SRC_RSTICK_PRESS},
        };
        static const struct { uint32_t mask; int source; } sticks[] = {
            {retrou::kStickSrcUp, SRC_LSTICK_UP},
            {retrou::kStickSrcDown, SRC_LSTICK_DOWN},
            {retrou::kStickSrcLeft, SRC_LSTICK_LEFT},
            {retrou::kStickSrcRight, SRC_LSTICK_RIGHT},
            {retrou::kStickRSrcUp, SRC_RSTICK_UP},
            {retrou::kStickRSrcDown, SRC_RSTICK_DOWN},
            {retrou::kStickRSrcLeft, SRC_RSTICK_LEFT},
            {retrou::kStickRSrcRight, SRC_RSTICK_RIGHT},
        };
        for (int action = 0; action < N64_INPUTS; ++action) {
            uint32_t sources = 0;
            const uint32_t mask = actions.sources[action];
            for (const auto &button : buttons)
                if (mask & (pro ? button.wpad : button.vpad))
                    sources |= 1u << button.source;
            for (const auto &stick : sticks)
                if (mask & stick.mask) sources |= 1u << stick.source;
            result.sources[action] = sources;
        }
        return result;
    }

    std::unique_ptr<N64Session> session_;
    std::string romPath_;
    int vpadPort_ = 0;
    int wpadPorts_[2] = {-1, -1};
    std::array<int, 4> devices_{{0, -1, -1, -1}};
    std::array<int, 4> channels_{{-1, -1, -1, -1}};
    retrou::CoreActionMap vpadActionMap_{};
    retrou::CoreActionMap proActionMap_{};
    bool running_ = false;
};
