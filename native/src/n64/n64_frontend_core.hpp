#pragma once

#include "n64_session.hpp"
#include "rpl_core.hpp" // RetroU's physical action maps and frontend types

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
        if (vpadPort_ >= 0) session_->assign(vpadPort_, 0);
        for (int port = 0; port < 2; ++port)
            if (wpadPorts_[port] >= 0) {
                session_->setChannel(port, wpadPorts_[port]);
                session_->assign(port == vpadPort_ ? 1 : port, port + 1);
            }
        running_ = session_->start(romPath_, root);
        return running_;
    }
    void requestStop() { if (running_) session_->requestStop(); }
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
    void setVpadActionMap(const retrou::CoreActionMap &) {}
    void setWpadActionMap(const retrou::CoreActionMap &) {}
    void setInputEnabled(bool) {}
    void setVideoOptions(int, int, int) {}
    void setExternalAudioDevice(SDL_AudioDeviceID, int) {}
    void setAudioOptions(int, int) {}

private:
    std::unique_ptr<N64Session> session_;
    std::string romPath_;
    int vpadPort_ = 0;
    int wpadPorts_[2] = {-1, -1};
    bool running_ = false;
};
