#include "legacy_config.h"

#include "config_sanitize.h"

#include "cameraunlock/config/ini_reader.h"
#include "cameraunlock/logging/file_log.h"

#include <fstream>

namespace RedEclipseHeadTracking::legacy {

namespace {

namespace Log = ::cameraunlock::logging;

bool FileExists(const char* path) {
    std::ifstream f(path);
    return f.good();
}

}  // namespace

ReadStatus Read(const char* iniPath, Config& cfg) {
    if (!FileExists(iniPath)) {
        return ReadStatus::Absent;
    }

    cameraunlock::IniReader ini;
    if (!ini.Open(iniPath)) {
        Log::Line("ERROR: Failed to open INI: %s", iniPath);
        return ReadStatus::OpenFailed;
    }

    cfg.enabled_on_startup = ini.ReadBool("General", "EnableOnStartup", kDefaultEnableOnStartup);
    int port = ini.ReadInt("General", "Port", kDefaultPort);
    if (port < kMinPort || port > kMaxPort) {
        Log::Line("ERROR: INI port %d out of range %d-%d", port, kMinPort, kMaxPort);
        return ReadStatus::PortRefused;
    }
    cfg.udp_port = static_cast<uint16_t>(port);
    cfg.data_freshness_ms = ini.ReadInt("General", "DataFreshnessMs", kDefaultDataFreshnessMs);
    cfg.world_space_yaw = ini.ReadBool("General", "WorldSpaceYaw", kDefaultWorldSpaceYaw);

    auto sanitize = [](const char* name, float raw, float clean) {
        if (raw != clean) {
            Log::Line("WARN: INI %s value %.4f out of range or non-finite; using %.4f",
                      name, raw, clean);
        }
        return clean;
    };

    float rawSensYaw   = ini.ReadFloat("Sensitivity", "Yaw",   kDefaultSensitivity);
    float rawSensPitch = ini.ReadFloat("Sensitivity", "Pitch", kDefaultSensitivity);
    float rawSensRoll  = ini.ReadFloat("Sensitivity", "Roll",  kDefaultSensitivity);
    cfg.sens_yaw   = sanitize("Sensitivity.Yaw",   rawSensYaw,   SanitizeSensitivity(rawSensYaw));
    cfg.sens_pitch = sanitize("Sensitivity.Pitch", rawSensPitch, SanitizeSensitivity(rawSensPitch));
    cfg.sens_roll  = sanitize("Sensitivity.Roll",  rawSensRoll,  SanitizeSensitivity(rawSensRoll));
    cfg.invert_yaw   = ini.ReadBool("Sensitivity", "InvertYaw",   kDefaultInvertYaw);
    cfg.invert_pitch = ini.ReadBool("Sensitivity", "InvertPitch", kDefaultInvert);
    cfg.invert_roll  = ini.ReadBool("Sensitivity", "InvertRoll",  kDefaultInvertRoll);

    float rawLocalSmoothing  = ini.ReadFloat("Smoothing", "LocalSmoothing",  kDefaultLocalSmoothing);
    float rawRemoteSmoothing = ini.ReadFloat("Smoothing", "RemoteSmoothing", kDefaultRemoteSmoothing);
    float rawDeadzone  = ini.ReadFloat("Smoothing", "DeadzoneDeg", kDefaultDeadzoneDeg);
    cfg.local_smoothing  = sanitize("Smoothing.LocalSmoothing",  rawLocalSmoothing,
                                    SanitizeSmoothing(rawLocalSmoothing, kDefaultLocalSmoothing));
    cfg.remote_smoothing = sanitize("Smoothing.RemoteSmoothing", rawRemoteSmoothing,
                                    SanitizeSmoothing(rawRemoteSmoothing, kDefaultRemoteSmoothing));
    cfg.deadzone_deg = sanitize("Smoothing.DeadzoneDeg", rawDeadzone,  SanitizeDeadzone(rawDeadzone));

    cfg.position_enabled = ini.ReadBool("Position", "Enabled", kDefaultPositionEnabled);
    cfg.pos_sens_x = ini.ReadFloat("Position", "SensitivityX", kDefaultPosSens);
    cfg.pos_sens_y = ini.ReadFloat("Position", "SensitivityY", kDefaultPosSens);
    cfg.pos_sens_z = ini.ReadFloat("Position", "SensitivityZ", kDefaultPosSens);
    cfg.pos_limit_x = ini.ReadFloat("Position", "LimitX", kDefaultPosLimitX);
    cfg.pos_limit_y = ini.ReadFloat("Position", "LimitY", kDefaultPosLimitY);
    cfg.pos_limit_z = ini.ReadFloat("Position", "LimitZ", kDefaultPosLimitZ);
    cfg.pos_limit_z_back = ini.ReadFloat("Position", "LimitZBack", kDefaultPosLimitZBack);
    cfg.position_scale = ini.ReadFloat("Position", "PositionScale", kDefaultPositionScale);
    cfg.invert_pos_x = ini.ReadBool("Position", "InvertX", kDefaultInvertPosX);
    cfg.invert_pos_y = ini.ReadBool("Position", "InvertY", kDefaultInvert);
    cfg.invert_pos_z = ini.ReadBool("Position", "InvertZ", kDefaultInvertPosZ);

    cfg.vk_toggle     = ini.ReadHex("Hotkeys", "Toggle",    kDefaultVkToggle);
    cfg.vk_cycle_mode = ini.ReadHex("Hotkeys", "CycleMode", kDefaultVkCycleMode);
    cfg.vk_yaw_mode   = ini.ReadHex("Hotkeys", "YawMode",   kDefaultVkYawMode);
    cfg.chord_toggle     = ini.ReadBool("Hotkeys", "ChordToggle",    kDefaultChord);
    cfg.chord_cycle_mode = ini.ReadBool("Hotkeys", "ChordCycleMode", kDefaultChord);
    cfg.chord_yaw_mode   = ini.ReadBool("Hotkeys", "ChordYawMode",   kDefaultChord);

    return ReadStatus::Read;
}

std::vector<cameraunlock::config::LegacyKey> ReadKeys() {
    return {
        {"General", "EnableOnStartup"},  {"General", "Port"},
        {"General", "DataFreshnessMs"},  {"General", "WorldSpaceYaw"},
        {"Sensitivity", "Yaw"},          {"Sensitivity", "Pitch"},         {"Sensitivity", "Roll"},
        {"Sensitivity", "InvertYaw"},    {"Sensitivity", "InvertPitch"},   {"Sensitivity", "InvertRoll"},
        {"Smoothing", "LocalSmoothing"}, {"Smoothing", "RemoteSmoothing"}, {"Smoothing", "DeadzoneDeg"},
        {"Position", "Enabled"},
        {"Position", "SensitivityX"},    {"Position", "SensitivityY"},     {"Position", "SensitivityZ"},
        {"Position", "LimitX"},          {"Position", "LimitY"},           {"Position", "LimitZ"},
        {"Position", "LimitZBack"},      {"Position", "PositionScale"},
        {"Position", "InvertX"},         {"Position", "InvertY"},          {"Position", "InvertZ"},
        {"Hotkeys", "Toggle"},           {"Hotkeys", "CycleMode"},         {"Hotkeys", "YawMode"},
        {"Hotkeys", "ChordToggle"},      {"Hotkeys", "ChordCycleMode"},    {"Hotkeys", "ChordYawMode"},
    };
}

}  // namespace RedEclipseHeadTracking::legacy
