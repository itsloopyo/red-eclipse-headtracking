#include "config.h"

#include "legacy_config/legacy_config.h"

#include "cameraunlock/config/head_tracking_config_table.h"
#include "cameraunlock/input/key_bindings.h"
#include "cameraunlock/tracking/tracking_mode.h"

#include <string>
#include <utility>
#include <vector>

namespace RedEclipseHeadTracking {

namespace {

namespace cfg = cameraunlock::config;
using cameraunlock::input::KeyBinding;
using cameraunlock::input::KeyModifiers;

// A legacy hotkey code and its Ctrl+Shift chord switch as one key list: the code's binding
// when it is a key code, then the chord. A code outside 0x01-0xFE imports as unbound (N1).
std::string KeyList(int vk, bool chord, char letter, const char* key, std::vector<cfg::DroppedValue>& dropped) {
    cfg::LegacyVirtualKeyToBindings(vk, "Hotkeys", key, dropped);
    std::vector<KeyBinding> bindings;
    if (vk >= 0x01 && vk <= 0xFE) bindings.push_back({KeyModifiers::kNone, vk});
    if (chord) bindings.push_back({KeyModifiers::kCtrl | KeyModifiers::kShift, letter});
    return cameraunlock::input::FormatKeyBindings(bindings);
}

cfg::ImportResult Import(const cfg::LegacyInput& input, Config& out) {
    legacy::Config c;
    const legacy::ReadStatus status = legacy::Read(input.ansi_path.c_str(), c);
    switch (status) {
        case legacy::ReadStatus::OpenFailed:
            return cfg::ImportResult::Refused("the file could not be opened, so head tracking stays off as it did before");
        case legacy::ReadStatus::PortRefused:
            return cfg::ImportResult::Refused(
                "[General] Port is not a whole number from 1024 to 65535, which the last version refused too, "
                "so head tracking stays off until the Port line is fixed");
        case legacy::ReadStatus::Read:
        case legacy::ReadStatus::Absent:
            break;
    }

    std::vector<cfg::DroppedValue> dropped;
    std::vector<cfg::PoseShapingValue> shaping;
    const Config defaults = MakeConfigTable().defaults();

    out.enable_on_startup = c.enabled_on_startup;
    out.udp_port = c.udp_port;
    out.data_freshness_ms = c.data_freshness_ms;
    out.world_space_yaw = c.world_space_yaw;

    // [Position] Enabled chose only the mode the session started in: the cycle key reached
    // every mode either way.
    const cameraunlock::TrackingModeChannels mode = cameraunlock::EncodeTrackingMode(
        c.position_enabled ? cameraunlock::TrackingMode::RotationAndPosition
                           : cameraunlock::TrackingMode::RotationOnly);
    out.rotation_enabled = mode.rotation_enabled;
    out.position_enabled = mode.position_enabled;

    // The frozen reader already held both to finite values in [0, 1].
    out.local_smoothing = c.local_smoothing;
    out.position.local_smoothing = c.local_smoothing;
    out.remote_smoothing = c.remote_smoothing;
    out.position.remote_smoothing = c.remote_smoothing;

    // LimitY bounded both directions, so it becomes both explicit values.
    out.position.limit_x = cfg::LegacyFiniteOrDefault(c.pos_limit_x, defaults.position.limit_x, "Position", "LimitX", dropped);
    const float limitY = cfg::LegacyFiniteOrDefault(c.pos_limit_y, defaults.position.limit_y, "Position", "LimitY", dropped);
    out.position.limit_y = limitY;
    out.position.limit_y_down = limitY;
    out.position.limit_z = cfg::LegacyFiniteOrDefault(c.pos_limit_z, defaults.position.limit_z, "Position", "LimitZ", dropped);
    out.position.limit_z_back =
        cfg::LegacyFiniteOrDefault(c.pos_limit_z_back, defaults.position.limit_z_back, "Position", "LimitZBack", dropped);

    // The shipped yaw and roll inversions and the 8 units to the metre are the axis
    // conversion itself, now in ToEnginePose (engine_pose.h); every other shipped value was
    // identity. A value the player changed is dropped.
    const auto shape = [&](auto value, auto shipped, const char* section, const char* key) {
        cfg::LegacyPoseShaping(value, shipped, section, key, shaping, dropped);
    };
    shape(c.sens_yaw, legacy::kDefaultSensitivity, "Sensitivity", "Yaw");
    shape(c.sens_pitch, legacy::kDefaultSensitivity, "Sensitivity", "Pitch");
    shape(c.sens_roll, legacy::kDefaultSensitivity, "Sensitivity", "Roll");
    shape(c.invert_yaw, legacy::kDefaultInvertYaw, "Sensitivity", "InvertYaw");
    shape(c.invert_pitch, legacy::kDefaultInvert, "Sensitivity", "InvertPitch");
    shape(c.invert_roll, legacy::kDefaultInvertRoll, "Sensitivity", "InvertRoll");
    shape(c.deadzone_deg, legacy::kDefaultDeadzoneDeg, "Smoothing", "DeadzoneDeg");
    shape(c.pos_sens_x, legacy::kDefaultPosSens, "Position", "SensitivityX");
    shape(c.pos_sens_y, legacy::kDefaultPosSens, "Position", "SensitivityY");
    shape(c.pos_sens_z, legacy::kDefaultPosSens, "Position", "SensitivityZ");
    shape(c.position_scale, legacy::kDefaultPositionScale, "Position", "PositionScale");
    shape(c.invert_pos_x, legacy::kDefaultInvertPosX, "Position", "InvertX");
    shape(c.invert_pos_y, legacy::kDefaultInvert, "Position", "InvertY");
    shape(c.invert_pos_z, legacy::kDefaultInvertPosZ, "Position", "InvertZ");

    out.toggle_key_name = KeyList(c.vk_toggle, c.chord_toggle, 'Y', "Toggle", dropped);
    out.cycle_tracking_mode_key_name = KeyList(c.vk_cycle_mode, c.chord_cycle_mode, 'G', "CycleMode", dropped);
    out.yaw_mode_key_name = KeyList(c.vk_yaw_mode, c.chord_yaw_mode, 'H', "YawMode", dropped);

    return status == legacy::ReadStatus::Absent ? cfg::ImportResult::Absent(std::move(dropped), std::move(shaping))
                                                : cfg::ImportResult::Imported(std::move(dropped), std::move(shaping));
}

}  // namespace

cfg::ConfigTable<Config> MakeConfigTable() {
    using C = cfg::schema::Concept;
    cfg::ConfigTable<Config> table = cfg::HeadTrackingConfigTable<Config>(
        {C::UdpPort, C::EnableOnStartup, C::WorldSpaceYaw, C::RotationEnabled, C::DataFreshnessMs,
         C::LocalSmoothing, C::RemoteSmoothing, C::PositionEnabled, C::PositionLimitX, C::PositionLimitY,
         C::PositionLimitYDown, C::PositionLimitZ, C::PositionLimitZBack, C::ToggleKey, C::CycleTrackingModeKey,
         C::YawModeKey});
    table.Select(C::WorldSpaceYaw).Writable()
        .Select(C::RotationEnabled).Writable()
        .Select(C::PositionEnabled).Writable();
    return table;
}

cfg::LegacyImport<Config> MakeLegacyImport() {
    return {&Import, legacy::ReadKeys()};
}

cfg::ConfigOwnerOptions<Config> MakeConfigOwnerOptions(const std::wstring& folder, cfg::DefaultsFile defaults) {
    const auto wide = [](const char* name) { return std::wstring(name, name + std::char_traits<char>::length(name)); };
    cfg::ConfigOwnerOptions<Config> options;
    options.path = folder + wide(kConfigFileName);
    options.legacy_path = folder + wide(kLegacyConfigFileName);
    options.table = MakeConfigTable();
    options.import = MakeLegacyImport();
    options.header.display_name = kConfigDisplayName;
    options.defaults = std::move(defaults);
    return options;
}

}  // namespace RedEclipseHeadTracking
