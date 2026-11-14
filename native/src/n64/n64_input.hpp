#pragma once

#include "input.h" // evaluador genérico del motor N64; sin lector de Our64

#include <array>
#include <cstdint>
#include <vpad/input.h>

class N64InputBridge {
public:
    N64InputBridge();

    // -1: libre, 0: GamePad, 1..4: slot lógico de mando externo de RetroU.
    void assign(int port, int device);
    void setChannel(int slot, int channel); // canal físico KPAD o -1
    void setMap(int type, const input_map &map);
    void poll(uint32_t ports[4]); // una vez por VI, en el hilo del motor
    void rumble(int port, bool enabled);
    void stopRumble();

private:
    std::array<int, 4> devices_{{0, -1, -1, -1}};
    std::array<int, 4> channels_{{-1, -1, -1, -1}};
    std::array<int, 4> types_{{-1, -1, -1, -1}};
    std::array<int, 4> present_{{-1, -1, -1, -1}};
    std::array<bool, 4> rumble_{{false, false, false, false}};
    std::array<bool, 4> motor_on_{{false, false, false, false}};
    std::array<input_map, INPUT_TYPES> maps_{};
    VPADStatus last_pad_{};
    bool gamepad_present_ = false;
};
