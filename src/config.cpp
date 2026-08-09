#include "config.h"

#include "config_sanitize.h"
#include "logging.h"

#include "cameraunlock/config/ini_reader.h"

#include <cstdio>
#include <fstream>

namespace RedEclipseHeadTracking {

namespace {

// Single source of truth for the INI defaults and the port validation bounds,
// shared by the writer (WriteDefaultIni) and the reader (LoadOrCreate) so the
// two cannot drift apart. The float-typed defaults widen to double for the
// WriteDouble calls and match ReadFloat exactly on the read side.
constexpr bool  kDefaultEnableOnStartup = true;
constexpr int   kDefaultPort            = 4242;
constexpr int   kMinPort                = 1024;
constexpr int   kMaxPort                = 65535;
constexpr int   kDefaultDataFreshnessMs = 500;
constexpr bool  kDefaultWorldSpaceYaw   = true;
constexpr float kDefaultSensitivity     = 1.0f;
constexpr bool  kDefaultInvert          = false;
// The head transform is built with right-handed rotations about Cube's camera
// axes, which runs opposite to the tracker on yaw and roll. Correcting it here
// keeps the INI as the one place a user has to look to flip an axis.
constexpr bool  kDefaultInvertYaw       = true;
constexpr bool  kDefaultInvertRoll      = true;
constexpr bool  kDefaultInvertPosX      = false;
constexpr bool  kDefaultInvertPosZ      = false;
constexpr float kDefaultSmoothing       = 0.0f;
constexpr float kDefaultDeadzoneDeg     = 0.0f;
constexpr int   kDefaultVkRecenter      = 0x24; // VK_HOME
constexpr int   kDefaultVkToggle        = 0x23; // VK_END
constexpr int   kDefaultVkCycleMode     = 0x21; // VK_PRIOR (Page Up)
constexpr int   kDefaultVkYawMode       = 0x22; // VK_NEXT (Page Down)
constexpr bool  kDefaultChord           = true;

constexpr bool  kDefaultPositionEnabled = true;
constexpr float kDefaultPosSens         = 1.0f;
constexpr float kDefaultPosLimitX       = 0.30f;
constexpr float kDefaultPosLimitY       = 0.20f;
constexpr float kDefaultPosLimitZ       = 0.40f;
constexpr float kDefaultPosLimitZBack   = 0.10f;
constexpr float kDefaultPosSmoothing    = 0.15f;
constexpr float kDefaultPositionScale   = 8.0f;

bool FileExists(const char* path) {
    std::ifstream f(path);
    return f.good();
}

void WriteDefaultIni(const char* path) {
    cameraunlock::IniWriter w;
    if (!w.Open(path)) return;
    w.WriteComment(" Red Eclipse - Head Tracking configuration");
    w.WriteComment(" Lives next to redeclipse.exe in bin\\amd64\\.");
    w.WriteBlankLine();
    w.WriteSection("General");
    w.WriteBool("EnableOnStartup", kDefaultEnableOnStartup);
    w.WriteInt("Port", kDefaultPort);
    w.WriteInt("DataFreshnessMs", kDefaultDataFreshnessMs);
    w.WriteComment(" Yaw mode: true = horizon-locked yaw (default), false = camera-local.");
    w.WriteBool("WorldSpaceYaw", kDefaultWorldSpaceYaw);
    w.WriteBlankLine();
    w.WriteSection("Sensitivity");
    w.WriteDouble("Yaw", kDefaultSensitivity);
    w.WriteDouble("Pitch", kDefaultSensitivity);
    w.WriteDouble("Roll", kDefaultSensitivity);
    w.WriteBool("InvertYaw", kDefaultInvertYaw);
    w.WriteBool("InvertPitch", kDefaultInvert);
    w.WriteBool("InvertRoll", kDefaultInvertRoll);
    w.WriteBlankLine();
    w.WriteSection("Smoothing");
    w.WriteDouble("Smoothing", kDefaultSmoothing);
    w.WriteDouble("DeadzoneDeg", kDefaultDeadzoneDeg);
    w.WriteBlankLine();
    w.WriteSection("Position");
    w.WriteComment(" 6DOF positional tracking. PositionScale = world units per metre of head translation (Cube uses 8).");
    w.WriteBool("Enabled", kDefaultPositionEnabled);
    w.WriteDouble("SensitivityX", kDefaultPosSens);
    w.WriteDouble("SensitivityY", kDefaultPosSens);
    w.WriteDouble("SensitivityZ", kDefaultPosSens);
    w.WriteDouble("LimitX", kDefaultPosLimitX);
    w.WriteDouble("LimitY", kDefaultPosLimitY);
    w.WriteDouble("LimitZ", kDefaultPosLimitZ);
    w.WriteDouble("LimitZBack", kDefaultPosLimitZBack);
    w.WriteDouble("Smoothing", kDefaultPosSmoothing);
    w.WriteDouble("PositionScale", kDefaultPositionScale);
    w.WriteBool("InvertX", kDefaultInvertPosX);
    w.WriteBool("InvertY", kDefaultInvert);
    w.WriteBool("InvertZ", kDefaultInvertPosZ);
    w.WriteBlankLine();
    w.WriteSection("Hotkeys");
    w.WriteComment(" Virtual-key codes. Defaults: Home (recenter), End (toggle), Page Up (cycle tracking mode), Page Down (yaw mode).");
    w.WriteHex("Recenter", kDefaultVkRecenter);
    w.WriteHex("Toggle", kDefaultVkToggle);
    w.WriteHex("CycleMode", kDefaultVkCycleMode);
    w.WriteHex("YawMode", kDefaultVkYawMode);
    w.WriteComment(" Chord alternatives: Ctrl+Shift+T (recenter), Ctrl+Shift+Y (toggle), Ctrl+Shift+G (cycle tracking mode), Ctrl+Shift+H (yaw mode).");
    w.WriteBool("ChordRecenter", kDefaultChord);
    w.WriteBool("ChordToggle", kDefaultChord);
    w.WriteBool("ChordCycleMode", kDefaultChord);
    w.WriteBool("ChordYawMode", kDefaultChord);
    w.Close();
}

}

bool Config::LoadOrCreate(const char* iniPath) {
    if (!FileExists(iniPath)) {
        WriteDefaultIni(iniPath);
    }

    cameraunlock::IniReader ini;
    if (!ini.Open(iniPath)) {
        Log::Line("ERROR: Failed to open INI: %s", iniPath);
        return false;
    }

    enabled_on_startup = ini.ReadBool("General", "EnableOnStartup", kDefaultEnableOnStartup);
    int port = ini.ReadInt("General", "Port", kDefaultPort);
    if (port < kMinPort || port > kMaxPort) {
        Log::Line("ERROR: INI port %d out of range %d-%d", port, kMinPort, kMaxPort);
        return false;
    }
    udp_port = static_cast<uint16_t>(port);
    data_freshness_ms = ini.ReadInt("General", "DataFreshnessMs", kDefaultDataFreshnessMs);
    world_space_yaw = ini.ReadBool("General", "WorldSpaceYaw", kDefaultWorldSpaceYaw);

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
    sens_yaw   = sanitize("Sensitivity.Yaw",   rawSensYaw,   SanitizeSensitivity(rawSensYaw));
    sens_pitch = sanitize("Sensitivity.Pitch", rawSensPitch, SanitizeSensitivity(rawSensPitch));
    sens_roll  = sanitize("Sensitivity.Roll",  rawSensRoll,  SanitizeSensitivity(rawSensRoll));
    invert_yaw   = ini.ReadBool("Sensitivity", "InvertYaw",   kDefaultInvertYaw);
    invert_pitch = ini.ReadBool("Sensitivity", "InvertPitch", kDefaultInvert);
    invert_roll  = ini.ReadBool("Sensitivity", "InvertRoll",  kDefaultInvertRoll);

    float rawSmoothing = ini.ReadFloat("Smoothing", "Smoothing",   kDefaultSmoothing);
    float rawDeadzone  = ini.ReadFloat("Smoothing", "DeadzoneDeg", kDefaultDeadzoneDeg);
    smoothing    = sanitize("Smoothing.Smoothing",  rawSmoothing, SanitizeSmoothing(rawSmoothing));
    deadzone_deg = sanitize("Smoothing.DeadzoneDeg", rawDeadzone,  SanitizeDeadzone(rawDeadzone));

    position_enabled = ini.ReadBool("Position", "Enabled", kDefaultPositionEnabled);
    pos_sens_x = ini.ReadFloat("Position", "SensitivityX", kDefaultPosSens);
    pos_sens_y = ini.ReadFloat("Position", "SensitivityY", kDefaultPosSens);
    pos_sens_z = ini.ReadFloat("Position", "SensitivityZ", kDefaultPosSens);
    pos_limit_x = ini.ReadFloat("Position", "LimitX", kDefaultPosLimitX);
    pos_limit_y = ini.ReadFloat("Position", "LimitY", kDefaultPosLimitY);
    pos_limit_z = ini.ReadFloat("Position", "LimitZ", kDefaultPosLimitZ);
    pos_limit_z_back = ini.ReadFloat("Position", "LimitZBack", kDefaultPosLimitZBack);
    pos_smoothing = sanitize("Position.Smoothing",
                             ini.ReadFloat("Position", "Smoothing", kDefaultPosSmoothing),
                             SanitizeSmoothing(ini.ReadFloat("Position", "Smoothing", kDefaultPosSmoothing)));
    position_scale = ini.ReadFloat("Position", "PositionScale", kDefaultPositionScale);
    invert_pos_x = ini.ReadBool("Position", "InvertX", kDefaultInvertPosX);
    invert_pos_y = ini.ReadBool("Position", "InvertY", kDefaultInvert);
    invert_pos_z = ini.ReadBool("Position", "InvertZ", kDefaultInvertPosZ);

    vk_recenter   = ini.ReadHex("Hotkeys", "Recenter",  kDefaultVkRecenter);
    vk_toggle     = ini.ReadHex("Hotkeys", "Toggle",    kDefaultVkToggle);
    vk_cycle_mode = ini.ReadHex("Hotkeys", "CycleMode", kDefaultVkCycleMode);
    vk_yaw_mode   = ini.ReadHex("Hotkeys", "YawMode",   kDefaultVkYawMode);
    chord_recenter   = ini.ReadBool("Hotkeys", "ChordRecenter",  kDefaultChord);
    chord_toggle     = ini.ReadBool("Hotkeys", "ChordToggle",    kDefaultChord);
    chord_cycle_mode = ini.ReadBool("Hotkeys", "ChordCycleMode", kDefaultChord);
    chord_yaw_mode   = ini.ReadBool("Hotkeys", "ChordYawMode",   kDefaultChord);

    return true;
}

}
