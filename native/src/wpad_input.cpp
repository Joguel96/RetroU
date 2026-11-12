#include "wpad_input.hpp"

#include <padscore/kpad.h>
#include <padscore/wpad.h>

#include <algorithm>
#include <array>

// Wii Remotes and their extensions all come through KPAD (same as Our64):
// KPADReadEx fills one KPADStatus per channel and puts the extension data in
// .classic / .pro depending on extensionType. The Classic bit masks are
// identical to the Pro ones (WPAD_CLASSIC_BUTTON_* == WPAD_PRO_BUTTON_*), so
// consumers only ever see the WPAD_PRO_* family. A bare Wii Remote uses the
// top-level .hold with WPAD_BUTTON_* masks (different layout).

namespace {
std::array<WpadSnapshot, 4> g_states;
std::array<int, 4> g_empty_reads = {0, 0, 0, 0};
bool g_initialized = false;
constexpr int kEmptyReadsToClear = 60; // ~1 s a 60 fps

int typeForExtension(uint8_t ext)
{
    switch (ext) {
        case WPAD_EXT_PRO_CONTROLLER: return WPAD_TYPE_PRO;
        case WPAD_EXT_CLASSIC:
        case WPAD_EXT_MPLUS_CLASSIC:  return WPAD_TYPE_CLASSIC;
        case WPAD_EXT_NUNCHUK:
        case WPAD_EXT_MPLUS_NUNCHUK:  return WPAD_TYPE_NUNCHUK;
        case WPAD_EXT_CORE:
        case WPAD_EXT_MPLUS:          return WPAD_TYPE_WIIMOTE;
        default: return WPAD_TYPE_NONE; // balance board, instruments, etc.
    }
}
}

void wpad_input_init()
{
    if (g_initialized) return;
    KPADInit();
    // Without these the system drops the controllers as soon as we take the
    // foreground: URCC is the Wii U Pro Controller support and WiiRemote the
    // Wii Remote + Classic one (both off by default; Our64 enables them too).
    WPADEnableURCC(TRUE);
    WPADEnableWiiRemote(TRUE);
    g_initialized = true;
}

void wpad_input_poll()
{
    if (!g_initialized) return;
    for (int ch = 0; ch < 4; ++ch) {
        KPADStatus s{};
        KPADError err = KPAD_ERROR_NO_SAMPLES;
        const uint32_t n = KPADReadEx(static_cast<KPADChan>(ch), &s, 1, &err);
        const int type = (n > 0 && err == KPAD_ERROR_OK)
            ? typeForExtension(s.extensionType) : WPAD_TYPE_NONE;
        if (type != WPAD_TYPE_NONE) {
            WpadSnapshot snap{};
            snap.connected = true;
            snap.type = type;
            switch (type) {
                case WPAD_TYPE_PRO:
                    snap.buttons = s.pro.hold;
                    snap.left_x = static_cast<int>(s.pro.leftStick.x * 32767.0f);
                    snap.left_y = static_cast<int>(s.pro.leftStick.y * 32767.0f);
                    snap.right_x = static_cast<int>(s.pro.rightStick.x * 32767.0f);
                    snap.right_y = static_cast<int>(s.pro.rightStick.y * 32767.0f);
                    break;
                case WPAD_TYPE_CLASSIC:
                    snap.buttons = s.classic.hold;
                    snap.left_x = static_cast<int>(s.classic.leftStick.x * 32767.0f);
                    snap.left_y = static_cast<int>(s.classic.leftStick.y * 32767.0f);
                    snap.right_x = static_cast<int>(s.classic.rightStick.x * 32767.0f);
                    snap.right_y = static_cast<int>(s.classic.rightStick.y * 32767.0f);
                    break;
                case WPAD_TYPE_NUNCHUK:
                    // The Wiimote part uses the top-level hold; the stick
                    // comes from the nunchuk.
                    snap.buttons = s.hold;
                    snap.left_x = static_cast<int>(s.nunchuk.stick.x * 32767.0f);
                    snap.left_y = static_cast<int>(s.nunchuk.stick.y * 32767.0f);
                    break;
                default: // WIIMOTE
                    snap.buttons = s.hold;
                    break;
            }
            g_states[ch] = snap;
            g_empty_reads[ch] = 0;
        } else {
            // An empty read is normal (the sample queue only refills between
            // frames, and this poll may be the second one in a frame): keep
            // the last state so a single empty read does not drop the
            // controller. Only a sustained silence clears it.
            if (++g_empty_reads[ch] >= kEmptyReadsToClear) {
                g_states[ch] = WpadSnapshot{};
                g_empty_reads[ch] = kEmptyReadsToClear;
            }
        }
    }
}

const std::array<WpadSnapshot, 4> &wpad_input_states()
{
    return g_states;
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
