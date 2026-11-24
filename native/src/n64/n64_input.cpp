#include "n64_input.hpp"

#include "wpad_input.hpp"

#include <padscore/wpad.h>
#include <vpad/input.h>
#include <cstring>

extern "C" void player_port_present(int port, int present);

namespace {
constexpr uint32_t source(int bit) { return 1u << bit; }

input_raw gamepadRaw(const VPADStatus &pad)
{
    input_raw raw{};
    static constexpr struct { uint32_t physical; int src; } buttons[] = {
        {VPAD_BUTTON_A, SRC_A}, {VPAD_BUTTON_B, SRC_B},
        {VPAD_BUTTON_X, SRC_X}, {VPAD_BUTTON_Y, SRC_Y},
        {VPAD_BUTTON_L, SRC_L}, {VPAD_BUTTON_R, SRC_R},
        {VPAD_BUTTON_ZL, SRC_ZL}, {VPAD_BUTTON_ZR, SRC_ZR},
        {VPAD_BUTTON_PLUS, SRC_PLUS}, {VPAD_BUTTON_MINUS, SRC_MINUS},
        {VPAD_BUTTON_UP, SRC_UP}, {VPAD_BUTTON_DOWN, SRC_DOWN},
        {VPAD_BUTTON_LEFT, SRC_LEFT}, {VPAD_BUTTON_RIGHT, SRC_RIGHT},
        {VPAD_BUTTON_STICK_L, SRC_LSTICK_PRESS},
        {VPAD_BUTTON_STICK_R, SRC_RSTICK_PRESS},
    };
    for (const auto &button : buttons)
        if (pad.hold & button.physical) raw.buttons |= source(button.src);
    raw.lx = pad.leftStick.x;
    raw.ly = pad.leftStick.y;
    raw.rx = pad.rightStick.x;
    raw.ry = pad.rightStick.y;
    return raw;
}

input_raw externalRaw(const WpadSnapshot &pad)
{
    input_raw raw{};
    if (pad.type == WPAD_TYPE_PRO || pad.type == WPAD_TYPE_CLASSIC) {
        static constexpr struct { uint32_t physical; int src; } buttons[] = {
            {WPAD_PRO_BUTTON_A, SRC_A}, {WPAD_PRO_BUTTON_B, SRC_B},
            {WPAD_PRO_BUTTON_X, SRC_X}, {WPAD_PRO_BUTTON_Y, SRC_Y},
            {WPAD_PRO_BUTTON_L, SRC_L}, {WPAD_PRO_BUTTON_R, SRC_R},
            {WPAD_PRO_BUTTON_ZL, SRC_ZL}, {WPAD_PRO_BUTTON_ZR, SRC_ZR},
            {WPAD_PRO_BUTTON_PLUS, SRC_PLUS}, {WPAD_PRO_BUTTON_MINUS, SRC_MINUS},
            {WPAD_PRO_BUTTON_UP, SRC_UP}, {WPAD_PRO_BUTTON_DOWN, SRC_DOWN},
            {WPAD_PRO_BUTTON_LEFT, SRC_LEFT}, {WPAD_PRO_BUTTON_RIGHT, SRC_RIGHT},
            {WPAD_PRO_BUTTON_STICK_L, SRC_LSTICK_PRESS},
            {WPAD_PRO_BUTTON_STICK_R, SRC_RSTICK_PRESS},
        };
        for (const auto &button : buttons)
            if (pad.buttons & button.physical) raw.buttons |= source(button.src);
    } else {
        const bool sideways = pad.type == WPAD_TYPE_WIIMOTE;
        static constexpr struct { uint32_t physical; int upright, side; } buttons[] = {
            {WPAD_BUTTON_UP, SRC_UP, SRC_LEFT},
            {WPAD_BUTTON_DOWN, SRC_DOWN, SRC_RIGHT},
            {WPAD_BUTTON_LEFT, SRC_LEFT, SRC_DOWN},
            {WPAD_BUTTON_RIGHT, SRC_RIGHT, SRC_UP},
            {WPAD_BUTTON_A, SRC_A, SRC_A}, {WPAD_BUTTON_B, SRC_B, SRC_B},
            {WPAD_BUTTON_1, SRC_1, SRC_1}, {WPAD_BUTTON_2, SRC_2, SRC_2},
            {WPAD_BUTTON_PLUS, SRC_PLUS, SRC_PLUS},
            {WPAD_BUTTON_MINUS, SRC_MINUS, SRC_MINUS},
        };
        for (const auto &button : buttons)
            if (pad.buttons & button.physical)
                raw.buttons |= source(sideways ? button.side : button.upright);
        if (pad.extension_buttons & WPAD_NUNCHUK_BUTTON_C) raw.buttons |= source(SRC_C);
        if (pad.extension_buttons & WPAD_NUNCHUK_BUTTON_Z) raw.buttons |= source(SRC_Z);
    }
    raw.lx = pad.left_x / 32767.0f;
    raw.ly = pad.left_y / 32767.0f;
    raw.rx = pad.right_x / 32767.0f;
    raw.ry = pad.right_y / 32767.0f;
    return raw;
}

int mapType(int type)
{
    switch (type) {
    case WPAD_TYPE_PRO: return INPUT_PRO;
    case WPAD_TYPE_CLASSIC: return INPUT_CLASSIC;
    case WPAD_TYPE_NUNCHUK: return INPUT_NUNCHUK;
    default: return INPUT_WIIMOTE;
    }
}

uint32_t navigationBits(uint32_t buttons, bool pro, bool sideways, float stickY)
{
    uint32_t bits = 0;
    if (pro) {
        if (buttons & WPAD_PRO_BUTTON_A) bits |= 1;
        if (buttons & WPAD_PRO_BUTTON_B) bits |= 2;
        if (buttons & WPAD_PRO_BUTTON_UP) bits |= 4;
        if (buttons & WPAD_PRO_BUTTON_DOWN) bits |= 8;
    } else {
        if (buttons & WPAD_BUTTON_A) bits |= 1;
        if (buttons & WPAD_BUTTON_B) bits |= 2;
        if (buttons & (sideways ? WPAD_BUTTON_RIGHT : WPAD_BUTTON_UP)) bits |= 4;
        if (buttons & (sideways ? WPAD_BUTTON_LEFT : WPAD_BUTTON_DOWN)) bits |= 8;
    }
    if (stickY > 0.55f) bits |= 4;
    if (stickY < -0.55f) bits |= 8;
    return bits;
}
}

N64InputBridge::N64InputBridge()
{
    for (int type = 0; type < INPUT_TYPES; ++type)
        input_default_map(type, &maps_[type]);
}

void N64InputBridge::assign(int port, int device)
{
    if (port < 0 || port >= 4 || device < -1 || device > 4) return;
    if (devices_[port] != device) stopRumble();
    for (int other = 0; other < 4; ++other)
        if (other != port && device >= 0 && devices_[other] == device)
            devices_[other] = -1;
    devices_[port] = device;
}

void N64InputBridge::setChannel(int slot, int channel)
{
    if (slot >= 0 && slot < 4 && channel >= -1 && channel < 4) {
        const int old = channels_[slot];
        if (old >= 0 && old != channel) {
            for (int port = 0; port < 4; ++port)
                if (devices_[port] == slot + 1 && motor_on_[port]) {
                    WPADControlMotor(static_cast<WPADChan>(old), false);
                    motor_on_[port] = false;
                }
        }
        channels_[slot] = channel;
        if (channel >= 0) {
            const auto &external = wpad_input_states();
            if (external[channel].connected) types_[slot] = external[channel].type;
        }
    }
}

void N64InputBridge::setMap(int type, const input_map &map)
{
    if (type >= 0 && type < INPUT_TYPES)
        maps_[type] = map;
}

void N64InputBridge::poll(uint32_t ports[4], bool suppressConfirm)
{
    wpad_input_poll();
    const auto &external = wpad_input_states();
    for (int slot = 0; slot < 4; ++slot) {
        const int channel = channels_[slot];
        if (channel < 0) continue;
        if (external[channel].connected &&
            (types_[slot] < 0 || external[channel].type == types_[slot])) {
            types_[slot] = external[channel].type;
            continue;
        }
        for (int port = 0; port < 4; ++port)
            if (devices_[port] == slot + 1 && motor_on_[port]) {
                WPADControlMotor(static_cast<WPADChan>(channel), false);
                motor_on_[port] = false;
            }
        channels_[slot] = -1;
    }
    for (int channel = 0; channel < 4; ++channel) {
        if (!external[channel].connected) continue;
        bool claimed = false;
        for (int slot = 0; slot < 4; ++slot)
            if (channels_[slot] == channel) claimed = true;
        if (claimed) continue;
        int target = -1;
        for (int slot = 0; slot < 4; ++slot)
            if (channels_[slot] < 0 && types_[slot] == external[channel].type) {
                target = slot;
                break;
            }
        if (target < 0) for (int slot = 0; slot < 4; ++slot)
            if (channels_[slot] < 0 && types_[slot] < 0) {
                target = slot;
                break;
            }
        if (target >= 0) {
            channels_[target] = channel;
            types_[target] = external[channel].type;
        }
    }
    VPADStatus pad{};
    VPADReadError error = VPAD_READ_NO_SAMPLES;
    VPADRead(VPAD_CHAN_0, &pad, 1, &error);
    if (error == VPAD_READ_SUCCESS) {
        last_pad_ = pad;
        gamepad_present_ = true;
    } else if (error == VPAD_READ_INVALID_CONTROLLER) {
        gamepad_present_ = false;
        std::memset(&last_pad_, 0, sizeof(last_pad_));
    }
    for (int port = 0; port < 4; ++port) {
        const int device = devices_[port];
        const int channel = device > 0 ? channels_[device - 1] : -1;
        const bool connected = device == 0 ? gamepad_present_ :
            device > 0 && channel >= 0 && external[channel].connected;
        const int nowPresent = connected ? 1 : 0;
        if (present_[port] != nowPresent) {
            player_port_present(port, nowPresent);
            present_[port] = nowPresent;
        }
        if (!connected) { ports[port] = 0; continue; }
        if (device == 0) {
            input_raw raw = gamepadRaw(last_pad_);
            if (suppressConfirm) raw.buttons &= ~(source(SRC_A) | source(SRC_B));
            ports[port] = input_eval(&maps_[INPUT_GAMEPAD], &raw);
        } else {
            const WpadSnapshot &state = external[channel];
            input_raw raw = externalRaw(state);
            if (suppressConfirm) raw.buttons &= ~(source(SRC_A) | source(SRC_B));
            ports[port] = input_eval(&maps_[mapType(state.type)], &raw);
        }
    }
    int gamepadPort = -1;
    for (int port = 0; port < 4; ++port)
        if (devices_[port] == 0) gamepadPort = port;
    if (gamepad_present_ && gamepadPort >= 0 && rumble_[gamepadPort]) {
        static const uint8_t pattern[15] = {0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
        VPADControlMotor(VPAD_CHAN_0, const_cast<uint8_t *>(pattern), sizeof(pattern));
    }
}

void N64InputBridge::rumble(int port, bool enabled)
{
    if (port < 0 || port >= 4) return;
    rumble_[port] = enabled;
    const int device = devices_[port];
    if (device == 0) {
        if (!enabled) VPADStopMotor(VPAD_CHAN_0);
    } else if (device > 0) {
        const int channel = channels_[device - 1];
        if (channel >= 0 && motor_on_[port] != enabled) {
            WPADControlMotor(static_cast<WPADChan>(channel), enabled);
            motor_on_[port] = enabled;
        }
    }
}

void N64InputBridge::stopRumble()
{
    for (int port = 0; port < 4; ++port) rumble(port, false);
}

uint32_t N64InputBridge::menuPressed()
{
    uint32_t pressed = 0;
    uint32_t bits = 0;
    if (gamepad_present_) {
        if (last_pad_.hold & VPAD_BUTTON_A) bits |= 1;
        if (last_pad_.hold & VPAD_BUTTON_B) bits |= 2;
        if (last_pad_.hold & VPAD_BUTTON_UP || last_pad_.leftStick.y > 0.55f) bits |= 4;
        if (last_pad_.hold & VPAD_BUTTON_DOWN || last_pad_.leftStick.y < -0.55f) bits |= 8;
        if (last_pad_.hold & VPAD_BUTTON_LEFT) bits |= 16;
        if (last_pad_.hold & VPAD_BUTTON_RIGHT) bits |= 32;
    }
    pressed |= bits & ~last_menu_bits_[0];
    last_menu_bits_[0] = bits;
    const auto &external = wpad_input_states();
    for (int ch = 0; ch < 4; ++ch) {
        const auto &pad = external[ch];
        const bool pro = pad.type == WPAD_TYPE_PRO || pad.type == WPAD_TYPE_CLASSIC;
        bits = pad.connected ? navigationBits(pad.buttons, pro,
            pad.type == WPAD_TYPE_WIIMOTE, pad.left_y / 32767.0f) : 0;
        if (pad.connected) {
            const bool sideways = pad.type == WPAD_TYPE_WIIMOTE;
            const uint32_t left = pro ? static_cast<uint32_t>(WPAD_PRO_BUTTON_LEFT) :
                                  static_cast<uint32_t>(sideways ? WPAD_BUTTON_UP : WPAD_BUTTON_LEFT);
            const uint32_t right = pro ? static_cast<uint32_t>(WPAD_PRO_BUTTON_RIGHT) :
                                   static_cast<uint32_t>(sideways ? WPAD_BUTTON_DOWN : WPAD_BUTTON_RIGHT);
            if (pad.buttons & left) bits |= 16;
            if (pad.buttons & right) bits |= 32;
        }
        pressed |= bits & ~last_menu_bits_[ch + 1];
        last_menu_bits_[ch + 1] = bits;
    }
    return pressed;
}

bool N64InputBridge::menuConfirmHeld() const
{
    if (gamepad_present_ && (last_pad_.hold & (VPAD_BUTTON_A | VPAD_BUTTON_B)))
        return true;
    const auto &external = wpad_input_states();
    for (const auto &pad : external) {
        if (!pad.connected) continue;
        const bool pro = pad.type == WPAD_TYPE_PRO || pad.type == WPAD_TYPE_CLASSIC;
        if (pad.buttons & (pro ? WPAD_PRO_BUTTON_A | WPAD_PRO_BUTTON_B :
                                WPAD_BUTTON_A | WPAD_BUTTON_B)) return true;
    }
    return false;
}

bool N64InputBridge::homePressed()
{
    bool pressed = false;
    bool held = gamepad_present_ && (last_pad_.hold & VPAD_BUTTON_HOME);
    pressed |= held && !last_home_[0];
    last_home_[0] = held;
    const auto &external = wpad_input_states();
    for (int ch = 0; ch < 4; ++ch) {
        const auto &pad = external[ch];
        const bool pro = pad.type == WPAD_TYPE_PRO || pad.type == WPAD_TYPE_CLASSIC;
        const uint32_t home = pro ? static_cast<uint32_t>(WPAD_PRO_BUTTON_HOME) :
                                    static_cast<uint32_t>(WPAD_BUTTON_HOME);
        held = pad.connected && (pad.buttons & home);
        pressed |= held && !last_home_[ch + 1];
        last_home_[ch + 1] = held;
    }
    return pressed;
}
