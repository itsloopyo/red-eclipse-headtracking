#pragma once

#include <cstdint>

namespace RedEclipseHeadTracking {

struct Config {
    bool enabled_on_startup = true;
    uint16_t udp_port = 4242;

    float sens_yaw = 1.0f;
    float sens_pitch = 1.0f;
    float sens_roll = 1.0f;
    bool invert_yaw = true;
    bool invert_pitch = false;
    bool invert_roll = true;

    float smoothing = 0.0f;
    float deadzone_deg = 0.0f;

    bool aim_decoupling = true;
    int data_freshness_ms = 500;

    // true = horizon-locked (world-space) yaw, false = camera-local yaw.
    bool world_space_yaw = true;

    // 6DOF positional tracking.
    bool position_enabled = true;
    float pos_sens_x = 1.0f;
    float pos_sens_y = 1.0f;
    float pos_sens_z = 1.0f;
    float pos_limit_x = 0.30f;
    float pos_limit_y = 0.20f;
    float pos_limit_z = 0.40f;
    float pos_limit_z_back = 0.10f;
    float pos_smoothing = 0.15f;
    bool invert_pos_x = false;
    bool invert_pos_y = false;
    bool invert_pos_z = false;
    // World units per metre of head translation. Cube's world is 8 units to the
    // metre - the engine's own distance readout divides by 8 to print metres.
    float position_scale = 8.0f;

    int vk_recenter = 0x24;    // VK_HOME
    int vk_toggle = 0x23;      // VK_END
    int vk_cycle_mode = 0x21;  // VK_PRIOR (Page Up)
    int vk_yaw_mode = 0x22;    // VK_NEXT (Page Down)
    bool chord_recenter = true;
    bool chord_toggle = true;
    bool chord_cycle_mode = true;
    bool chord_yaw_mode = true;

    bool LoadOrCreate(const char* iniPath);
};

}  // namespace RedEclipseHeadTracking
