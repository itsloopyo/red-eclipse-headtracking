#pragma once

#include <cstdint>

// v0.3.1's Config, copied out field by field so the differential test can read it without
// including the oracle's config.h, whose names clash with this build's. oracle_adapter.cpp
// is compiled with the oracle, where RedEclipseHeadTracking is renamed to re_oracle.

namespace oracle_api {

struct Config {
    bool ads_mode_is_default = true;
    int vk_ads_mode = 0;
    bool chord_ads_mode = false;
    bool enabled_on_startup = false;
    uint16_t udp_port = 0;

    float sens_yaw = 0.0f;
    float sens_pitch = 0.0f;
    float sens_roll = 0.0f;
    bool invert_yaw = false;
    bool invert_pitch = false;
    bool invert_roll = false;

    float local_smoothing = 0.0f;
    float remote_smoothing = 0.0f;
    float deadzone_deg = 0.0f;

    int data_freshness_ms = 0;
    bool world_space_yaw = false;

    bool position_enabled = false;
    float pos_sens_x = 0.0f;
    float pos_sens_y = 0.0f;
    float pos_sens_z = 0.0f;
    float pos_limit_x = 0.0f;
    float pos_limit_y = 0.0f;
    float pos_limit_z = 0.0f;
    float pos_limit_z_back = 0.0f;
    bool invert_pos_x = false;
    bool invert_pos_y = false;
    bool invert_pos_z = false;
    float position_scale = 0.0f;

    int vk_toggle = 0;
    int vk_cycle_mode = 0;
    int vk_yaw_mode = 0;
    bool chord_toggle = false;
    bool chord_cycle_mode = false;
    bool chord_yaw_mode = false;
};

// A default-constructed v0.3.1 Config.
Config Defaults();

// v0.3.1's Config::LoadOrCreate on the file at `iniPath`, which creates the file when it is
// missing, as that build did. False where that build did not start.
bool LoadOrCreate(const char* iniPath, Config& out);

}  // namespace oracle_api
