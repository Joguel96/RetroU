#pragma once

#include "n64_input.hpp"

#include <atomic>
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
    bool start(const std::string &romPath, const std::string &root);
    void requestStop();
    bool requestSave(int slot);
    bool requestLoad(int slot);
    bool finished() const;
    int join();
    int stateResult() const { return stateResult_.load(); }

private:
    static int threadMain(int argc, void *argv);
    static int onVi(void *user, unsigned vi, uint32_t ports[4]);
    static void onRumble(void *user, int port, int enabled);

    N64InputBridge input_;
    OSThread thread_{};
    alignas(16) uint8_t stack_[1024 * 1024]{};
    std::string romPath_;
    std::string root_;
    std::atomic<int> pendingSave_{-1};
    std::atomic<int> pendingLoad_{-1};
    std::atomic<int> stateResult_{0};
    std::atomic<bool> stopRequested_{false};
    bool started_ = false;
};
