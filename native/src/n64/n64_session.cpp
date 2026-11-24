#include "n64_session.hpp"

#include "n64_engine.h"
#include "n64_states.h"
#include "report.h"

#include <coreinit/codegen.h>
#include <coreinit/core.h>
#include <coreinit/time.h>
#include <algorithm>

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
    startupStage_ = 0;
    resumeCount_ = -1;
    romPath_ = romPath;
    root_ = root;
    pendingSave_ = pendingLoad_ = -1;
    stateResult_ = 0;
    stopRequested_ = false;
    menuPaused_ = false;
    menuRequest_ = 0;
    suppressConfirm_ = false;
    traceLastRaw_ = traceLastPort_ = 0;
    traceCount_ = 0;
    menuButtons_ = 0;
    const uint32_t codegenCore = OSGetCodegenCore();
    if (!OSCreateThread(&thread_, threadMain, 0, this,
                        stack_ + sizeof(stack_), sizeof(stack_), 16,
                        static_cast<OSThreadAttributes>(1u << codegenCore)))
        return false;
    started_ = true;
    OSSetThreadName(&thread_, "RetroU N64 CPU");
    resumeCount_ = OSResumeThread(&thread_);
    return true;
}

void N64Session::requestStop()
{
    if (!started_) return;
    stopRequested_ = true;
    menuPaused_ = false;
    n64_engine_request_stop();
}

bool N64Session::requestSave(int slot, const uint16_t *thumbnail)
{
    if (!started_ || slot < 0 || slot >= 4 || pendingSave_ >= 0 || pendingLoad_ >= 0)
        return false;
    stateResult_ = 0;
    if (thumbnail) std::copy(thumbnail, thumbnail + pendingThumbnail_.size(),
                             pendingThumbnail_.begin());
    else pendingThumbnail_.fill(0);
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

bool N64Session::readStateThumbnail(int slot, uint16_t *pixels) const
{
    return started_ && n64_states_read_thumb(slot, pixels) != 0;
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
    session->startupStage_ = 1;
    N64EngineHost host{};
    host.on_vi = onVi;
    host.on_rumble = onRumble;
    host.on_stage = onStage;
    host.user = session;
    return n64_engine_play(session->romPath_.c_str(), session->root_.c_str(), &host);
}

void N64Session::onStage(void *user, unsigned stage)
{
    static_cast<N64Session *>(user)->startupStage_ = stage;
}

int N64Session::onVi(void *user, unsigned vi, uint32_t ports[4])
{
    auto *session = static_cast<N64Session *>(user);
    session->input_.poll(ports, session->suppressConfirm_);
    const uint32_t raw = session->input_.gamepadButtons();
    if (vi == 1 || (session->traceCount_ < 32 &&
        (raw != session->traceLastRaw_ || ports[0] != session->traceLastPort_))) {
        report_line("N64 input VI=%u raw=%08X port1=%08X paused=%d suppress=%d",
                    vi, raw, ports[0], session->menuPaused_.load(),
                    session->suppressConfirm_);
        if (vi != 1) ++session->traceCount_;
        session->traceLastRaw_ = raw;
        session->traceLastPort_ = ports[0];
    }
    const bool homePressed = session->input_.homePressed();
    if (session->suppressConfirm_ && !session->input_.menuConfirmHeld())
        session->suppressConfirm_ = false;
    session->input_.menuPressed(); // keep held buttons from becoming menu presses
    if (session->menuPaused_)
        for (int port = 0; port < 4; ++port) ports[port] = 0;
    if (homePressed && !session->menuPaused_) {
        session->menuRequest_ = 1;
        session->menuPaused_ = true;
        session->input_.stopRumble();
        for (int port = 0; port < 4; ++port) ports[port] = 0;
    }
    const bool enteredMenu = session->menuPaused_;
    while (session->menuPaused_ && !session->stopRequested_) {
        uint32_t menuPorts[4]{};
        session->input_.poll(menuPorts);
        session->input_.homePressed();
        session->menuButtons_.fetch_or(session->input_.menuPressed());
        const int save = session->pendingSave_.exchange(-1);
        if (save >= 0) session->stateResult_ =
            n64_states_request_save(save, session->pendingThumbnail_.data());
        const int state = n64_states_result();
        if (state) session->stateResult_ = state;
        if (n64_states_save_pending()) {
            // Let Not64 reach its next dispatcher boundary. It captures at
            // the beginning of an interrupt, before the VI event is handled.
            return 0;
        }
        OSSleepTicks(OSMillisecondsToTicks(16));
    }
    if (session->stopRequested_) return 1;
    if (enteredMenu) {
        // Only the VI that returns from the menu drops the dismissing input.
        // Clearing every VI here would leave the game without any controls.
        session->suppressConfirm_ = session->input_.menuConfirmHeld();
        session->traceCount_ = 0;
        session->traceLastRaw_ = session->traceLastPort_ = 0;
        report_line("N64 input: menu reanudado; captura de 32 cambios activada");
        for (int port = 0; port < 4; ++port) ports[port] = 0;
    }
    const int save = session->pendingSave_.exchange(-1);
    if (save >= 0) session->stateResult_ =
        n64_states_request_save(save, session->pendingThumbnail_.data());
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
