#pragma once

#include "n64_input.hpp"

#include <atomic>
#include <array>
#include <coreinit/thread.h>
#include <cstdint>
#include <string>

// The RetroU frontend keeps ProcUI on its main thread while the N64 engine
// owns GX2 on its graphics thread. Call start only after releasing RetroU GX2.
class N64Session {
public:
    N64Session();
    ~N64Session();

    N64Session(const N64Session &) = delete;
    N64Session &operator=(const N64Session &) = delete;

    // Configure ports before start; the input bridge polls hot-plug on each VI.
    bool assign(int port, int device);
    bool setChannel(int slot, int channel);
    bool setMap(int type, const input_map &map);
    bool start(const std::string &romPath, const std::string &root);
    void requestStop();
    int takeMenuRequest() { return menuRequest_.exchange(0); } // 0: none; 1..5: device
    uint32_t takeMenuButtons() { return menuButtons_.exchange(0); }
    bool menuPaused() const { return menuPaused_.load(); }
    void resumeMenu() { menuPaused_ = false; }
    bool requestSave(int slot, const uint16_t *thumbnail);
    bool requestLoad(int slot);
    unsigned startupStage() const { return startupStage_.load(); }
    int resumeCount() const { return resumeCount_; }
    bool finished() const;
    int join();
    int stateResult() const { return stateResult_.load(); }
    bool readStateThumbnail(int slot, uint16_t *pixels) const;

private:
    static int threadMain(int argc, void *argv);
    static int onVi(void *user, unsigned vi, uint32_t ports[4]);
    static void onRumble(void *user, int port, int enabled);

    static void onStage(void *user, unsigned stage);
    std::atomic<unsigned> startupStage_{0};
    int resumeCount_ = -1;
    N64InputBridge input_;
    OSThread thread_{};
    alignas(16) uint8_t stack_[1024 * 1024]{};
    std::string romPath_;
    std::string root_;
    std::atomic<int> pendingSave_{-1};
    std::array<uint16_t, 160 * 120> pendingThumbnail_{};
    std::atomic<int> pendingLoad_{-1};
    std::atomic<int> stateResult_{0};
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> menuPaused_{false};
    std::atomic<int> menuRequest_{0};
    bool suppressConfirm_ = false;
    uint32_t traceLastRaw_ = 0;
    uint32_t traceLastPort_ = 0;
    int traceCount_ = 0;
    std::atomic<uint32_t> menuButtons_{0};
    bool started_ = false;
};
