#pragma once

#include <cstdint>
#include <vector>

struct WpadSnapshot {
    bool connected = false;
    uint32_t buttons = 0;
    int left_x = 0;
    int left_y = 0;
};

void wpad_input_init();
void wpad_input_poll();
std::vector<int> wpad_input_channels();
WpadSnapshot wpad_input_state(int channel);
