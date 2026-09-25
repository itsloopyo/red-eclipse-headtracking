#include "oracle_adapter.h"

#include "config.h"

namespace oracle_api {

namespace {

Config Copy(const re_oracle::Config& c) {
    Config out;
    out.ads_mode_is_default = c.ads_mode == cameraunlock::ads::kDefaultAdsMode;
    out.vk_ads_mode = c.vk_ads_mode;
    out.chord_ads_mode = c.chord_ads_mode;
    out.enabled_on_startup = c.enabled_on_startup;
    out.udp_port = c.udp_port;
    out.sens_yaw = c.sens_yaw;
    out.sens_pitch = c.sens_pitch;
    out.sens_roll = c.sens_roll;
    out.invert_yaw = c.invert_yaw;
    out.invert_pitch = c.invert_pitch;
    out.invert_roll = c.invert_roll;
    out.local_smoothing = c.local_smoothing;
    out.remote_smoothing = c.remote_smoothing;
    out.deadzone_deg = c.deadzone_deg;
    out.data_freshness_ms = c.data_freshness_ms;
    out.world_space_yaw = c.world_space_yaw;
    out.position_enabled = c.position_enabled;
    out.pos_sens_x = c.pos_sens_x;
    out.pos_sens_y = c.pos_sens_y;
    out.pos_sens_z = c.pos_sens_z;
    out.pos_limit_x = c.pos_limit_x;
    out.pos_limit_y = c.pos_limit_y;
    out.pos_limit_z = c.pos_limit_z;
    out.pos_limit_z_back = c.pos_limit_z_back;
    out.invert_pos_x = c.invert_pos_x;
    out.invert_pos_y = c.invert_pos_y;
    out.invert_pos_z = c.invert_pos_z;
    out.position_scale = c.position_scale;
    out.vk_toggle = c.vk_toggle;
    out.vk_cycle_mode = c.vk_cycle_mode;
    out.vk_yaw_mode = c.vk_yaw_mode;
    out.chord_toggle = c.chord_toggle;
    out.chord_cycle_mode = c.chord_cycle_mode;
    out.chord_yaw_mode = c.chord_yaw_mode;
    return out;
}

}  // namespace

Config Defaults() {
    return Copy(re_oracle::Config{});
}

bool LoadOrCreate(const char* iniPath, Config& out) {
    re_oracle::Config c;
    const bool usable = c.LoadOrCreate(iniPath);
    out = Copy(c);
    return usable;
}

}  // namespace oracle_api
