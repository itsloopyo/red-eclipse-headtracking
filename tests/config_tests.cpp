// CameraUnlock.ini and what the conversion moved into code: the committed file is the table's
// fresh render, the defaults every published build ran on map to the defaults, a first start
// creates the committed file, the toggles save only their own lines and leave Defaults.ini and
// RedEclipseHeadTracking.ini alone, End's row cannot be saved, and the axis conversion that
// replaced the shipped InvertYaw, InvertRoll and PositionScale gives the camera the pose those
// settings gave it.
//
// `--render-config <path>` writes the committed file instead (pixi run render-config).

#include "config.h"
#include "engine_pose.h"

#include "legacy_config/legacy_config.h"

#include "cameraunlock/processing/tracking_processor.h"
#include "cameraunlock/tracking/tracking_mode.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace RedEclipseHeadTracking;

namespace {

namespace cfg = cameraunlock::config;
namespace fs = std::filesystem;

int g_failures = 0;

void Check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what.c_str());
        ++g_failures;
    }
}

std::string ReadBytes(const std::wstring& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read a test file");
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteBytes(const std::wstring& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write a test file");
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

std::wstring Widen(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

// The file a first start creates: default on every row, since each one is a global concept.
std::string Rendered() {
    return cfg::RenderCanonicalFresh(MakeConfigTable(), {kConfigDisplayName});
}

void TestCommittedConfigIsRendered() {
    Check(ReadBytes(Widen(RE_COMMITTED_CONFIG)) == Rendered(),
          "config/RedEclipseHeadTracking.ini is the table's fresh render (pixi run render-config)");
}

// A scratch game folder, and a Defaults.ini of its own beside it that the first load creates.
struct Scratch {
    fs::path root;
    fs::path game;
    fs::path defaults;

    explicit Scratch(const char* name) {
        wchar_t temp[MAX_PATH];
        GetTempPathW(MAX_PATH, temp);
        root = fs::path(temp) / (std::string("red-eclipse-") + name + "-" + std::to_string(GetCurrentProcessId()));
        fs::remove_all(root);
        game = root / "bin" / "amd64";
        fs::create_directories(game);
        fs::create_directories(root / "user");
        defaults = root / "user" / "CameraUnlock" / "Defaults.ini";
    }
    ~Scratch() {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;

    cfg::ConfigOwnerOptions<Config> Options() const {
        return MakeConfigOwnerOptions(game.wstring() + L"\\", cfg::DefaultsFile::At(defaults.wstring()));
    }
};

std::vector<std::string> Listing(const fs::path& dir) {
    std::vector<std::string> names;
    for (const fs::directory_entry& entry : fs::directory_iterator(dir)) names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());
    return names;
}

// With neither file there, the first start creates CameraUnlock.ini as the committed file and
// Defaults.ini with the built-in values, and no RedEclipseHeadTracking.ini.
void TestFirstStartCreatesTheCommittedFile() {
    const Scratch s("config-first-start");
    const auto loaded = cfg::ConfigOwner<Config>(s.Options()).Load();
    Check(loaded.status == cfg::ConfigLoadStatus::Created, "a first start with no file is Created");
    Check(ReadBytes((s.game / "CameraUnlock.ini").wstring()) == ReadBytes(Widen(RE_COMMITTED_CONFIG)),
          "a first start creates config/RedEclipseHeadTracking.ini's bytes as CameraUnlock.ini");
    Check(Listing(s.game) == std::vector<std::string>{"CameraUnlock.ini"},
          "a first start creates CameraUnlock.ini and nothing else");
    Check(fs::exists(s.defaults), "a first start creates Defaults.ini");
    const auto table = MakeConfigTable();
    Check(cfg::RenderCanonical(table, loaded.config, {kConfigDisplayName}) ==
              cfg::RenderCanonical(table, table.defaults(), {kConfigDisplayName}),
          "a first start runs on the built-in values");
}

// A fresh install and an upgrade from any published build's defaults start the same: the map
// of the frozen defaults holds every row at the table's default, and every pose-shaping value
// they hold is the shipped one, which the axis conversion now does.
void TestLegacyDefaultsMapToTheDefaults() {
    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    const std::wstring missing = std::wstring(temp) + L"red-eclipse-no-such-folder\\" + Widen(kConfigFileName);
    const auto table = MakeConfigTable();
    Config mapped = table.defaults();
    const int size = WideCharToMultiByte(CP_ACP, 0, missing.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string ansi(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_ACP, 0, missing.c_str(), -1, ansi.data(), size, nullptr, nullptr);
    ansi.resize(static_cast<size_t>(size) - 1);
    const cfg::ImportResult result = MakeLegacyImport().run(cfg::LegacyInput{missing, ansi, false}, mapped);
    Check(result.status == cfg::ImportStatus::Absent && result.dropped.empty(), "the old defaults drop nothing");
    Check(result.pose_shaping.size() == 14, "every sensitivity, inversion, deadzone and scale is recorded");
    for (const cfg::PoseShapingValue& value : result.pose_shaping) {
        Check(value.folded, "[" + value.section + "] " + value.key + " at its shipped value is folded");
    }
    Check(cfg::RenderCanonical(table, mapped, {kConfigDisplayName}) ==
              cfg::RenderCanonical(table, table.defaults(), {kConfigDisplayName}),
          "the old defaults map to the defaults");
    Check(mapped.toggle_key_name == "End, Ctrl+Shift+Y" && mapped.cycle_tracking_mode_key_name == "PageUp, Ctrl+Shift+G" &&
              mapped.yaw_mode_key_name == "PageDown, Ctrl+Shift+H",
          "the old hotkeys and their chord switches become the fleet's key lists");
}

std::vector<std::string> Lines(const std::string& bytes) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < bytes.size()) {
        const size_t end = bytes.find("\r\n", start);
        lines.push_back(bytes.substr(start, end - start));
        start = end + 2;
    }
    return lines;
}

// The lines of `after` that differ from `before`, which must have as many lines.
std::vector<std::string> ChangedLines(const std::string& before, const std::string& after) {
    const std::vector<std::string> a = Lines(before);
    const std::vector<std::string> b = Lines(after);
    if (a.size() != b.size()) return {"a line was added or removed"};
    std::vector<std::string> changed;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) changed.push_back(b[i]);
    }
    return changed;
}

bool Contains(const std::vector<std::string>& lines, const std::string& text) {
    for (const std::string& line : lines) {
        if (line.find(text) != std::string::npos) return true;
    }
    return false;
}

// A save changes the lines of its rows and no other byte, writes a value over default, and
// touches neither Defaults.ini nor RedEclipseHeadTracking.ini; the yaw mode and the tracking
// mode persist, and End's row cannot be saved at all.
void TestTogglesSave() {
    const Scratch s("config-save");
    const std::wstring path = (s.game / "CameraUnlock.ini").wstring();
    const std::wstring legacyPath = (s.game / kLegacyConfigFileName).wstring();
    const std::string committed = ReadBytes(Widen(RE_COMMITTED_CONFIG));
    const std::string legacyBytes = "[General]\r\nEnableOnStartup=0\r\n";
    WriteBytes(path, committed);
    WriteBytes(legacyPath, legacyBytes);

    {
        cfg::ConfigOwner<Config> owner(s.Options());
        const auto loaded = owner.Load();
        Check(loaded.status == cfg::ConfigLoadStatus::Canonical, "the committed file loads as canonical");
        Check(loaded.config.enable_on_startup, "RedEclipseHeadTracking.ini is not read while CameraUnlock.ini exists");
        Check(Contains(loaded.log, "is left as it was and is not read"),
              "the log says RedEclipseHeadTracking.ini is not read while CameraUnlock.ini exists");
        const std::string defaultsBefore = ReadBytes(s.defaults.wstring());

        const cfg::ConfigSaveResult yaw = owner.Save([](Config& c) { c.world_space_yaw = false; });
        Check(yaw.status == cfg::ConfigSaveStatus::Saved, "the yaw mode saves");
        Check(Contains(yaw.log, "WorldSpaceYaw=false is now set for this game, and no longer follows Defaults.ini"),
              "the yaw save says WorldSpaceYaw stopped following Defaults.ini");
        const std::string afterYaw = ReadBytes(path);
        Check(ChangedLines(committed, afterYaw) == std::vector<std::string>{"WorldSpaceYaw=false"},
              "saving the yaw mode writes its value over default and changes nothing else");

        const auto rotationOnly = cameraunlock::EncodeTrackingMode(cameraunlock::TrackingMode::RotationOnly);
        Check(owner.Save([rotationOnly](Config& c) {
                  c.rotation_enabled = rotationOnly.rotation_enabled;
                  c.position_enabled = rotationOnly.position_enabled;
              }).status == cfg::ConfigSaveStatus::Saved,
              "the tracking mode saves");
        const std::string afterRotationOnly = ReadBytes(path);
        Check(ChangedLines(afterYaw, afterRotationOnly) ==
                  std::vector<std::string>{"RotationEnabled=true", "PositionEnabled=false"},
              "saving rotation only writes the mode pair over default and changes nothing else");

        const auto positionOnly = cameraunlock::EncodeTrackingMode(cameraunlock::TrackingMode::PositionOnly);
        Check(owner.Save([positionOnly](Config& c) {
                  c.rotation_enabled = positionOnly.rotation_enabled;
                  c.position_enabled = positionOnly.position_enabled;
              }).status == cfg::ConfigSaveStatus::Saved,
              "the third tracking mode saves");
        Check(ChangedLines(afterRotationOnly, ReadBytes(path)) ==
                  std::vector<std::string>{"RotationEnabled=false", "PositionEnabled=true"},
              "saving position only changes the mode pair and nothing else");

        bool refused = false;
        try {
            owner.Save([](Config& c) { c.enable_on_startup = false; });
        } catch (const std::logic_error&) {
            refused = true;
        }
        Check(refused, "EnableOnStartup is not Writable, so the End toggle cannot persist");

        Check(ReadBytes(s.defaults.wstring()) == defaultsBefore, "saving leaves Defaults.ini as it was");
        Check(ReadBytes(legacyPath) == legacyBytes, "saving leaves RedEclipseHeadTracking.ini as it was");
    }

    const auto again = cfg::ConfigOwner<Config>(s.Options()).Load();
    Check(again.status == cfg::ConfigLoadStatus::Canonical && again.diagnostics.empty() &&
              !again.config.world_space_yaw && !again.config.rotation_enabled && again.config.position_enabled &&
              again.config.enable_on_startup,
          "the saved yaw and tracking mode come back at the next start");
    Check((Listing(s.game) == std::vector<std::string>{"CameraUnlock.ini", kLegacyConfigFileName}),
          "the game folder holds CameraUnlock.ini and RedEclipseHeadTracking.ini and nothing else");
}

uint32_t Bits(float f) {
    uint32_t b;
    std::memcpy(&b, &f, sizeof(b));
    return b;
}

// v0.3.1 ran the processor with its shipped InvertYaw=true and InvertRoll=true, and scaled the
// lean by PositionScale=8. This build runs the processor at identity and makes the same
// conversion in ToEnginePose. Over a run of frames, through the processor's smoothing, the pose
// the camera gets is the same to the bit.
void TestShippedPoseShapingIsFolded() {
    cameraunlock::SensitivitySettings shipped;
    shipped.invert_yaw = legacy::kDefaultInvertYaw;
    shipped.invert_pitch = legacy::kDefaultInvert;
    shipped.invert_roll = legacy::kDefaultInvertRoll;
    cameraunlock::TrackingProcessor before;
    before.SetSensitivity(shipped);
    before.SetRemoteSmoothing(legacy::kDefaultRemoteSmoothing);
    before.SetIsRemoteConnection(true);
    cameraunlock::TrackingProcessor now;
    now.SetRemoteSmoothing(legacy::kDefaultRemoteSmoothing);
    now.SetIsRemoteConnection(true);

    const float zooms[] = {1.0f, 0.35f};
    for (int frame = 0; frame < 120; ++frame) {
        const float t = static_cast<float>(frame);
        const float yaw = 40.0f * std::sin(t * 0.07f);
        const float pitch = -25.0f * std::sin(t * 0.05f + 1.0f);
        const float roll = 15.0f * std::sin(t * 0.11f + 2.0f);
        const float x = 0.2f * std::sin(t * 0.03f);
        const float y = -0.1f * std::sin(t * 0.04f);
        const float z = 0.3f * std::sin(t * 0.09f);
        const cameraunlock::TrackingPose b = before.Process(yaw, pitch, roll, 1.0f / 60.0f);
        const cameraunlock::TrackingPose n = now.Process(yaw, pitch, roll, 1.0f / 60.0f);
        for (const float zoom : zooms) {
            // v0.3.1's HookedRecomputeCamera, with the pose its processor gave.
            HeadPose expected;
            expected.yaw_deg = cameraunlock::camera::ScaleAngleForZoom(b.yaw, zoom);
            expected.pitch_deg = cameraunlock::camera::ScaleAngleForZoom(b.pitch, zoom);
            expected.roll_deg = b.roll;
            expected.x = -x * legacy::kDefaultPositionScale * zoom;
            expected.y = y * legacy::kDefaultPositionScale * zoom;
            expected.z = -z * legacy::kDefaultPositionScale * zoom;

            const HeadPose actual = ToEnginePose(TrackedPose{n.pitch, n.yaw, n.roll, x, y, z}, true, zoom);
            const bool same = Bits(actual.yaw_deg) == Bits(expected.yaw_deg) &&
                              Bits(actual.pitch_deg) == Bits(expected.pitch_deg) &&
                              Bits(actual.roll_deg) == Bits(expected.roll_deg) && Bits(actual.x) == Bits(expected.x) &&
                              Bits(actual.y) == Bits(expected.y) && Bits(actual.z) == Bits(expected.z);
            Check(same, "frame " + std::to_string(frame) + " at zoom " + std::to_string(zoom) +
                            ": the camera gets v0.3.1's pose at its shipped settings");
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::strcmp(argv[1], "--render-config") == 0) {
            const std::string rendered = Rendered();
            std::ofstream out(argv[2], std::ios::binary | std::ios::trunc);
            out.write(rendered.data(), static_cast<std::streamsize>(rendered.size()));
            if (!out) {
                std::printf("could not write %s\n", argv[2]);
                return 1;
            }
            return 0;
        }

        TestCommittedConfigIsRendered();
        TestLegacyDefaultsMapToTheDefaults();
        TestFirstStartCreatesTheCommittedFile();
        TestTogglesSave();
        TestShippedPoseShapingIsFolded();
    } catch (const std::exception& e) {
        std::printf("FAIL: %s\n", e.what());
        return 1;
    }

    if (g_failures == 0) {
        std::printf("config tests: all passed\n");
        return 0;
    }
    std::printf("config tests: %d failure(s)\n", g_failures);
    return 1;
}
