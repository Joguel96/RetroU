#include "n64_session.hpp"

#include "n64_engine.h"
#include "n64_states.h"

#include <coreinit/codegen.h>
#include <coreinit/core.h>
#include <coreinit/time.h>

N64Session::N64Session() = default;

N64Session::~N64Session()
{
    if (started_) {
        requestStop();
        join();
    }
}

bool N64Session::assign(int port, int device)
{
    if (started_ || port < 0 || port >= 4 || device < -1 || device > 4)
        return false;
    input_.assign(port, device);
    return true;
}

bool N64Session::setChannel(int slot, int channel)
{
    if (started_ || slot < 0 || slot >= 4 || channel < -1 || channel >= 4)
        return false;
    input_.setChannel(slot, channel);
    return true;
}

bool N64Session::setMap(int type, const input_map &map)
{
    if (started_ || type < 0 || type >= INPUT_TYPES) return false;
    input_.setMap(type, map);
    return true;
}

bool N64Session::start(const std::string &romPath, const std::string &root)
{
    if (started_ || romPath.empty() || root.empty()) return false;
    romPath_ = romPath;
    root_ = root;
    pendingSave_ = pendingLoad_ = -1;
    stateResult_ = 0;
    stopRequested_ = false;
    const uint32_t codegenCore = OSGetCodegenCore();
    if (!OSCreateThread(&thread_, threadMain, 0, this,
                        stack_ + sizeof(stack_), sizeof(stack_), 16,
                        static_cast<OSThreadAttributes>(1u << codegenCore)))
        return false;
    started_ = true;
    OSSetThreadName(&thread_, "RetroU N64 CPU");
    OSResumeThread(&thread_);
    return true;
}

void N64Session::requestStop()
{
    if (!started_) return;
    stopRequested_ = true;
    n64_engine_request_stop();
}

bool N64Session::requestSave(int slot)
{
    if (!started_ || slot < 0 || slot >= 4 || pendingSave_ >= 0 || pendingLoad_ >= 0)
        return false;
    stateResult_ = 0;
    pendingSave_ = slot;
    return true;
}

bool N64Session::requestLoad(int slot)
{
    if (!started_ || slot < 0 || slot >= 4 || pendingSave_ >= 0 || pendingLoad_ >= 0)
        return false;
    stateResult_ = 0;
    pendingLoad_ = slot;
    return true;
}

bool N64Session::finished() const
{
    return started_ && OSIsThreadTerminated(const_cast<OSThread *>(&thread_));
}

int N64Session::join()
{
    if (!started_) return -1;
    int result = -1;
    OSJoinThread(&thread_, &result);
    started_ = false;
    input_.stopRumble();
    return result;
}

int N64Session::threadMain(int, void *argv)
{
    auto *session = static_cast<N64Session *>(argv);
    N64EngineHost host{};
    host.on_vi = onVi;
    host.on_rumble = onRumble;
    host.user = session;
    return n64_engine_play(session->romPath_.c_str(), session->root_.c_str(), &host);
}

int N64Session::onVi(void *user, unsigned, uint32_t ports[4])
{
    auto *session = static_cast<N64Session *>(user);
    session->input_.poll(ports);
    const int save = session->pendingSave_.exchange(-1);
    if (save >= 0) session->stateResult_ = n64_states_request_save(save);
    const int load = session->pendingLoad_.exchange(-1);
    if (load >= 0) session->stateResult_ = n64_states_request_load(load);
    const int state = n64_states_result();
    if (state) session->stateResult_ = state;
    return session->stopRequested_ ? 1 : 0;
}

void N64Session::onRumble(void *user, int port, int enabled)
{
    static_cast<N64Session *>(user)->input_.rumble(port, enabled != 0);
}
