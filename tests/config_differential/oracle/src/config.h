#pragma once

#include <cstdint>
#include <string>
#include "cameraunlock/ads/ads_mode.h"

#include "cameraunlock/data/position_settings.h"
#include "cameraunlock/math/smoothing_utils.h"

namespace RedEclipseHeadTracking {

struct Config {
    cameraunlock::ads::AdsMode ads_mode = cameraunlock::ads::kDefaultAdsMode;
    std::string ini_path;
    int vk_ads_mode = 0x2D;
    bool chord_ads_mode = true;
    bool enabled_on_startup = true;
    uint16_t udp_port = 4242;

    float sens_yaw = 1.0f;
    float sens_pitch = 1.0f;
    float sens_roll = 1.0f;
    bool invert_yaw = true;
    bool invert_pitch = false;
    bool invert_roll = true;

    // Smoothing is chosen per connection: local for a tracker on this machine
    // (loopback), remote for a device on the network. Both cover rotation and
    // position.
    float local_smoothing = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
    float remote_smoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);
    float deadzone_deg = 0.0f;

    int data_freshness_ms = 500;

    // true = horizon-locked (world-space) yaw, false = camera-local yaw.
    bool world_space_yaw = true;

    // 6DOF positional tracking.
    bool position_enabled = true;
    float pos_sens_x = 1.0f;
    float pos_sens_y = 1.0f;
    float pos_sens_z = 1.0f;
    float pos_limit_x = cameraunlock::PositionSettings{}.limit_x;
    float pos_limit_y = cameraunlock::PositionSettings{}.limit_y;
    float pos_limit_z = cameraunlock::PositionSettings{}.limit_z;
    float pos_limit_z_back = cameraunlock::PositionSettings{}.limit_z_back;
    bool invert_pos_x = false;
    bool invert_pos_y = false;
    bool invert_pos_z = false;
    // World units per metre of head translation. Cube's world is 8 units to the
    // metre - the engine's own distance readout divides by 8 to print metres.
    float position_scale = 8.0f;

    int vk_toggle = 0x23;      // VK_END
    int vk_cycle_mode = 0x21;  // VK_PRIOR (Page Up)
    int vk_yaw_mode = 0x22;    // VK_NEXT (Page Down)
    bool chord_toggle = true;
    bool chord_cycle_mode = true;
    bool chord_yaw_mode = true;

    bool LoadOrCreate(const char* iniPath);
};

}  // namespace RedEclipseHeadTracking
