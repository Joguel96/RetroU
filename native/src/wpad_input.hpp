#pragma once

#include <cstdint>
#include <vector>
#include <array>

// Controller type reported by KPAD, for naming and mapping.
enum WpadType {
    WPAD_TYPE_NONE = 0,
    WPAD_TYPE_PRO = 1,      // Wii U Pro Controller
    WPAD_TYPE_CLASSIC = 2,  // Classic Controller (also Motion+ variant)
    WPAD_TYPE_WIIMOTE = 3,  // Wii Remote without extension
    WPAD_TYPE_NUNCHUK = 4,  // Wii Remote + Nunchuk
};

struct WpadSnapshot {
    bool connected = false;
    int type = WPAD_TYPE_NONE;
    uint32_t buttons = 0;
    int left_x = 0;
    int left_y = 0;
    int right_x = 0;
    int right_y = 0;
};

void wpad_input_init();
void wpad_input_poll();
const std::array<WpadSnapshot, 4> &wpad_input_states();
std::vector<int> wpad_input_channels();
WpadSnapshot wpad_input_state(int channel);
