#include "wpad_input.hpp"

#include <padscore/wpad.h>

#include <algorithm>
#include <array>

namespace {
std::array<WpadSnapshot, 4> g_states;
std::array<bool, 4> g_configured = {false, false, false, false};
bool g_initialized = false;
}

void wpad_input_init()
{
    if (g_initialized) return;
    WPADInit();
    g_initialized = true;
}

void wpad_input_poll()
{
    if (!g_initialized) return;
    for (int ch = 0; ch < 4; ++ch) {
        WPADExtensionType ext = WPAD_EXT_DEV_NOT_FOUND;
        WPADError error = WPADProbe(static_cast<WPADChan>(ch), &ext);
        if (error != WPAD_ERROR_NONE || ext != WPAD_EXT_PRO_CONTROLLER) {
            g_states[ch] = WpadSnapshot{};
            g_configured[ch] = false;
            continue;
        }
        if (!g_configured[ch]) {
            WPADSetDataFormat(static_cast<WPADChan>(ch), WPAD_FMT_PRO_CONTROLLER);
            g_configured[ch] = true;
        }
        WPADStatusProController status{};
        WPADRead(static_cast<WPADChan>(ch), reinterpret_cast<WPADStatus *>(&status));
        g_states[ch].connected = true;
        g_states[ch].buttons = status.buttons;
        g_states[ch].left_x = status.leftStick.x;
        g_states[ch].left_y = status.leftStick.y;
    }
}

std::vector<int> wpad_input_channels()
{
    std::vector<int> channels;
    for (int ch = 0; ch < 4; ++ch) {
        if (g_states[ch].connected) channels.push_back(ch);
    }
    return channels;
}

WpadSnapshot wpad_input_state(int channel)
{
    if (channel < 0 || channel >= 4) return WpadSnapshot{};
    return g_states[channel];
}
