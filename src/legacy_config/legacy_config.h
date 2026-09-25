#pragma once

#include "cameraunlock/config/legacy_import.h"

#include <cstdint>
#include <vector>

// The RedEclipseHeadTracking.ini reader as src/config.cpp held it at 1a266c2, the last commit
// before the canonical config format, frozen so an old file converts exactly as the builds
// before it read it. Never edited: a change here changes what a player's old file means.
//
// It differs from that commit's Config::LoadOrCreate in two ways only. It fills this frozen
// copy of that commit's Config rather than the runtime one, and it writes nothing: where the
// build created a missing file and read it back, this reports Absent and leaves the defaults,
// which is what reading the created file gave.
//
// The defaults are literals rather than core's constants, which is what they held at
// cameraunlock-core ee8cc72 and befb88e, so a later core default cannot move what an old file
// without the key means.

namespace RedEclipseHeadTracking::legacy {

constexpr bool  kDefaultEnableOnStartup = true;
constexpr int   kDefaultPort            = 4242;
constexpr int   kMinPort                = 1024;
constexpr int   kMaxPort                = 65535;
constexpr int   kDefaultDataFreshnessMs = 500;
constexpr bool  kDefaultWorldSpaceYaw   = true;
constexpr float kDefaultSensitivity     = 1.0f;
constexpr bool  kDefaultInvert          = false;
constexpr bool  kDefaultInvertYaw       = true;
constexpr bool  kDefaultInvertRoll      = true;
constexpr bool  kDefaultInvertPosX      = false;
constexpr bool  kDefaultInvertPosZ      = false;
constexpr float kDefaultLocalSmoothing  = static_cast<float>(0.0);
constexpr float kDefaultRemoteSmoothing = static_cast<float>(0.15);
constexpr float kDefaultDeadzoneDeg     = 0.0f;
constexpr int   kDefaultVkToggle        = 0x23;
constexpr int   kDefaultVkCycleMode     = 0x21;
constexpr int   kDefaultVkYawMode       = 0x22;
constexpr bool  kDefaultChord           = true;

constexpr bool  kDefaultPositionEnabled = true;
constexpr float kDefaultPosSens         = 1.0f;
constexpr float kDefaultPosLimitX       = 0.30f;
constexpr float kDefaultPosLimitY       = 0.20f;
constexpr float kDefaultPosLimitZ       = 0.40f;
constexpr float kDefaultPosLimitZBack   = 0.10f;
constexpr float kDefaultPositionScale   = 8.0f;

struct Config {
    bool enabled_on_startup = kDefaultEnableOnStartup;
    uint16_t udp_port = static_cast<uint16_t>(kDefaultPort);

    float sens_yaw = kDefaultSensitivity;
    float sens_pitch = kDefaultSensitivity;
    float sens_roll = kDefaultSensitivity;
    bool invert_yaw = kDefaultInvertYaw;
    bool invert_pitch = kDefaultInvert;
    bool invert_roll = kDefaultInvertRoll;

    float local_smoothing = kDefaultLocalSmoothing;
    float remote_smoothing = kDefaultRemoteSmoothing;
    float deadzone_deg = kDefaultDeadzoneDeg;

    int data_freshness_ms = kDefaultDataFreshnessMs;

    bool world_space_yaw = kDefaultWorldSpaceYaw;

    bool position_enabled = kDefaultPositionEnabled;
    float pos_sens_x = kDefaultPosSens;
    float pos_sens_y = kDefaultPosSens;
    float pos_sens_z = kDefaultPosSens;
    float pos_limit_x = kDefaultPosLimitX;
    float pos_limit_y = kDefaultPosLimitY;
    float pos_limit_z = kDefaultPosLimitZ;
    float pos_limit_z_back = kDefaultPosLimitZBack;
    bool invert_pos_x = kDefaultInvertPosX;
    bool invert_pos_y = kDefaultInvert;
    bool invert_pos_z = kDefaultInvertPosZ;
    float position_scale = kDefaultPositionScale;

    int vk_toggle = kDefaultVkToggle;
    int vk_cycle_mode = kDefaultVkCycleMode;
    int vk_yaw_mode = kDefaultVkYawMode;
    bool chord_toggle = kDefaultChord;
    bool chord_cycle_mode = kDefaultChord;
    bool chord_yaw_mode = kDefaultChord;
};

enum class ReadStatus {
    // The file was read into the Config.
    Read,
    // There is no file at the path. The build created one holding the defaults and read
    // that, so the Config holds the defaults.
    Absent,
    // The file vanished between the existence check and the open. The build did not start.
    OpenFailed,
    // [General] Port is outside kMinPort to kMaxPort, or not a number, which reads as 0.
    // The build did not start. The Config holds what was read before the port.
    PortRefused,
};

// Reads the file at the ANSI path through GetPrivateProfileStringA, as the build did.
// Writes nothing. Logs through cameraunlock::logging as the build did.
ReadStatus Read(const char* iniPath, Config& cfg);

// Every section and key Read takes a value from.
std::vector<cameraunlock::config::LegacyKey> ReadKeys();

}  // namespace RedEclipseHeadTracking::legacy
