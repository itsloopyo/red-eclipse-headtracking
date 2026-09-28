#include "config.h"

#include "legacy_config/legacy_config.h"

#include "cameraunlock/config/head_tracking_config_table.h"
#include "cameraunlock/tracking/tracking_mode.h"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace RedEclipseHeadTracking {

namespace {

namespace cfg = cameraunlock::config;
using cfg::schema::Concept;

// A legacy hotkey code and its Ctrl+Shift chord switch as one key list: the code's binding,
// then the chord. A code outside 0x01-0xFE (N1) or on a Ctrl, Shift or Alt key alone (N3)
// imports as unbound, and the player keeps the chord.
std::string KeyList(int vk, bool chord, char letter, const char* key, std::vector<cfg::DroppedValue>& dropped) {
    std::string list = cfg::LegacyVirtualKeyToBindings(vk, "Hotkeys", key, dropped);
    if (chord) list += (list.empty() ? "Ctrl+Shift+" : ", Ctrl+Shift+") + std::string(1, letter);
    return list;
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
    // The frozen reader read DataFreshnessMs and the limits with no range, so a value outside the
    // canonical row's range is clamped to its nearest end (N4), and a limit that is not finite
    // imports as its default (N2).
    out.data_freshness_ms =
        cfg::LegacyClampToRange<Concept::DataFreshnessMs>(c.data_freshness_ms, "General", "DataFreshnessMs", dropped);
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
    static_assert(cfg::schema::ConceptTraits<Concept::PositionLimitY>::kMin ==
                          cfg::schema::ConceptTraits<Concept::PositionLimitYDown>::kMin &&
                      cfg::schema::ConceptTraits<Concept::PositionLimitY>::kMax ==
                          cfg::schema::ConceptTraits<Concept::PositionLimitYDown>::kMax,
                  "one LimitY fills both vertical limit rows, so they take one range");
    out.position.limit_x =
        std::isfinite(c.pos_limit_x)
            ? cfg::LegacyClampToRange<Concept::PositionLimitX>(c.pos_limit_x, "Position", "LimitX", dropped)
            : cfg::LegacyFiniteOrDefault(c.pos_limit_x, defaults.position.limit_x, "Position", "LimitX", dropped);
    const float limitY =
        std::isfinite(c.pos_limit_y)
            ? cfg::LegacyClampToRange<Concept::PositionLimitY>(c.pos_limit_y, "Position", "LimitY", dropped)
            : cfg::LegacyFiniteOrDefault(c.pos_limit_y, defaults.position.limit_y, "Position", "LimitY", dropped);
    out.position.limit_y = limitY;
    out.position.limit_y_down = limitY;
    out.position.limit_z =
        std::isfinite(c.pos_limit_z)
            ? cfg::LegacyClampToRange<Concept::PositionLimitZ>(c.pos_limit_z, "Position", "LimitZ", dropped)
            : cfg::LegacyFiniteOrDefault(c.pos_limit_z, defaults.position.limit_z, "Position", "LimitZ", dropped);
    out.position.limit_z_back =
        std::isfinite(c.pos_limit_z_back)
            ? cfg::LegacyClampToRange<Concept::PositionLimitZBack>(c.pos_limit_z_back, "Position", "LimitZBack", dropped)
            : cfg::LegacyFiniteOrDefault(c.pos_limit_z_back, defaults.position.limit_z_back, "Position", "LimitZBack",
                                         dropped);

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

    // A setting the player never changed from what the legacy build shipped follows
    // Defaults.ini. LimitY stood for both vertical bounds, and each hotkey for its code and its
    // chord switch together. Each number is compared as read: one that is not finite follows
    // Defaults.ini (N2), and one N4 clamped is the player's.
    const legacy::Config shipped;
    cfg::LegacyFollowsDefaultsIni follows;
    follows.Setting(Concept::UdpPort, c.udp_port, shipped.udp_port);
    follows.Setting(Concept::EnableOnStartup, c.enabled_on_startup, shipped.enabled_on_startup);
    follows.Setting(Concept::WorldSpaceYaw, c.world_space_yaw, shipped.world_space_yaw);
    follows.TrackingMode(c.position_enabled, shipped.position_enabled);
    follows.Setting(Concept::DataFreshnessMs, c.data_freshness_ms, shipped.data_freshness_ms);
    follows.Setting(Concept::LocalSmoothing, c.local_smoothing, shipped.local_smoothing);
    follows.Setting(Concept::RemoteSmoothing, c.remote_smoothing, shipped.remote_smoothing);
    follows.Setting(Concept::PositionLimitX, c.pos_limit_x, shipped.pos_limit_x);
    follows.Setting(Concept::PositionLimitY, c.pos_limit_y, shipped.pos_limit_y);
    follows.Setting(Concept::PositionLimitYDown, c.pos_limit_y, shipped.pos_limit_y);
    follows.Setting(Concept::PositionLimitZ, c.pos_limit_z, shipped.pos_limit_z);
    follows.Setting(Concept::PositionLimitZBack, c.pos_limit_z_back, shipped.pos_limit_z_back);
    follows.Setting(Concept::ToggleKey, c.vk_toggle == shipped.vk_toggle && c.chord_toggle == shipped.chord_toggle);
    follows.Setting(Concept::CycleTrackingModeKey,
                    c.vk_cycle_mode == shipped.vk_cycle_mode && c.chord_cycle_mode == shipped.chord_cycle_mode);
    follows.Setting(Concept::YawModeKey, c.vk_yaw_mode == shipped.vk_yaw_mode && c.chord_yaw_mode == shipped.chord_yaw_mode);

    return status == legacy::ReadStatus::Absent
               ? cfg::ImportResult::Absent(std::move(dropped), std::move(shaping), follows.Concepts())
               : cfg::ImportResult::Imported(std::move(dropped), std::move(shaping), follows.Concepts());
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
