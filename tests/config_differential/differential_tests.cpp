// The config differential test. Every input is read three ways:
//
//   oracle     v0.3.1's reader (oracle/), the newest published build, and v0.3.1's startup code
//   import     the frozen reader in src/legacy_config/, and the startup code it ran under
//   migration  the config owner in a folder holding only RedEclipseHeadTracking.ini, the
//              legacy file, importing it into a new CameraUnlock.ini, then the canonical reader
//              and table on that file, and the startup code of this build
//
// Comparison 1, oracle against import, finds what a player updating from v0.3.1 sees change
// that the conversion did not cause. Every difference it may find is listed in
// kComparisonOneDifferences with the commit that made it; any other fails the test.
//
// Comparison 2, import against migration, is the proof for the migration: no difference but
// the approved ones, each of which the import must record as dropped. A sensitivity,
// inversion, deadzone or unit scale the player set away from its shipped value is dropped
// (pose_shaping); a limit that is not a finite number imports as its default (N2); a hotkey
// code outside 0x01-0xFE imports as unbound (N1). No default moved, so the no-file input has
// no difference either. A value the canonical row cannot hold (a DataFreshnessMs below 1, a
// finite position limit below 0 or above 10) has no approved rule: the owner defers that
// import, the session runs on what the import gave, and kUnrepresentable names them.
//
// Comparison 2 runs twice, once over a Defaults.ini at the built-in values and once over one a
// player changed, since the migration writes default exactly where the imported value equals
// what Defaults.ini gives. After every load RedEclipseHeadTracking.ini keeps its bytes, its
// write time and its attributes, Defaults.ini is never written, and the folder holds the
// legacy file and CameraUnlock.ini and nothing else (the legacy file alone when nothing was
// imported).
//
// The distinct migrated files are written beside the executable under migrated\, for
// lint-migrated.mjs to run core's canonical config lint over.
//
// Inputs: no file, an empty file, the first-run output of each published build (v0.2.0,
// v0.3.0 and v0.3.1, extracted once into inputs/; v0.1.0 was tagged but never released, and
// its reader and core pin are v0.2.0's), files v0.3.1 wrote after its ADS mode cycle saved,
// and core's corpus over v0.3.1's first-run output. No published build shipped a config file
// or a launcher seed: each created the file at first launch.

#include "config.h"
#include "legacy_config/legacy_config.h"
#include "oracle_adapter.h"

#include "cameraunlock/config/config_owner.h"
#include "cameraunlock/config/defaults_file.h"
#include "cameraunlock/config/legacy_import.h"
#include "cameraunlock/config/testing/ini_mutations.h"
#include "cameraunlock/input/key_binding_registration.h"
#include "cameraunlock/input/key_bindings.h"
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
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

namespace legacy = RedEclipseHeadTracking::legacy;
namespace cfg = cameraunlock::config;
using RedEclipseHeadTracking::Config;
using cameraunlock::TrackingMode;
using cameraunlock::config::testing::ChordSwitch;
using cameraunlock::config::testing::GenerateIniMutations;
using cameraunlock::config::testing::IniMutation;
using cameraunlock::config::testing::MutationKey;

int g_failures = 0;

void Fail(const std::string& input, const std::string& what) {
    if (g_failures < 50) std::printf("FAIL [%s]: %s\n", input.c_str(), what.c_str());
    ++g_failures;
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------

std::wstring Widen(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

std::string Narrow(const std::wstring& path) {
    const int size = WideCharToMultiByte(CP_ACP, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(size), 'x');
    WideCharToMultiByte(CP_ACP, 0, path.c_str(), -1, out.data(), size, nullptr, nullptr);
    out.resize(static_cast<size_t>(size) - 1);
    return out;
}

std::string ReadBytes(const std::wstring& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + Narrow(path));
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteBytes(const std::wstring& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write " + Narrow(path));
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

// Every file in the folder, name and bytes, for "the import changed nothing".
std::map<std::wstring, std::string> Snapshot(const std::wstring& dir) {
    std::map<std::wstring, std::string> files;
    WIN32_FIND_DATAW data;
    HANDLE find = FindFirstFileW((dir + L"\\*").c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot list the test folder");
    do {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        files[data.cFileName] = ReadBytes(dir + L"\\" + data.cFileName);
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return files;
}

// A file as the tests hold it to: its bytes, its last write time and its attributes.
struct FileStamp {
    std::string bytes;
    FILETIME written{};
    DWORD attributes = 0;

    bool operator==(const FileStamp& o) const {
        return bytes == o.bytes && CompareFileTime(&written, &o.written) == 0 && attributes == o.attributes;
    }
    bool operator!=(const FileStamp& o) const { return !(*this == o); }
};

FileStamp Stamp(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        throw std::runtime_error("cannot stat " + Narrow(path));
    }
    FileStamp s;
    s.bytes = ReadBytes(path);
    s.written = data.ftLastWriteTime;
    s.attributes = data.dwFileAttributes;
    return s;
}

void EmptyFolder(const std::wstring& dir) {
    for (const auto& [name, bytes] : Snapshot(dir)) {
        const std::wstring path = dir + L"\\" + name;
        SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
        if (!DeleteFileW(path.c_str())) throw std::runtime_error("cannot empty the test folder");
    }
}

std::wstring MakeFolder(const std::wstring& parent, const wchar_t* name) {
    const std::wstring dir = parent + L"\\" + name;
    if (!CreateDirectoryW(dir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
        throw std::runtime_error("cannot create the test folder");
    }
    EmptyFolder(dir);
    return dir;
}

// ---------------------------------------------------------------------------
// Hotkeys: what each build puts on its poller
// ---------------------------------------------------------------------------

enum class Action { Toggle, CycleMode, YawMode, AdsMode };

const char* ActionName(Action a) {
    switch (a) {
        case Action::Toggle: return "toggle";
        case Action::CycleMode: return "cycle mode";
        case Action::YawMode: return "yaw mode";
        case Action::AdsMode: return "ADS mode";
    }
    throw std::logic_error("action");
}

// One key the poller watches for an action, and the modifiers it fires with: 0 is NavGuarded
// (not while Ctrl and Shift are both held), kChord is ChordGuarded (while both are held).
struct Registration {
    Action action;
    int vk;
    unsigned modifiers;

    bool operator<(const Registration& o) const {
        return std::tie(action, vk, modifiers) < std::tie(o.action, o.vk, o.modifiers);
    }
    bool operator==(const Registration& o) const {
        return action == o.action && vk == o.vk && modifiers == o.modifiers;
    }
    bool operator!=(const Registration& o) const { return !(*this == o); }
};

using cameraunlock::input::KeyModifiers;
constexpr unsigned kNav = static_cast<unsigned>(KeyModifiers::kNone);
constexpr unsigned kChord = static_cast<unsigned>(KeyModifiers::kCtrl | KeyModifiers::kShift);

std::string Describe(const std::vector<Registration>& regs) {
    std::string out;
    for (const Registration& r : regs) {
        char buf[80];
        std::snprintf(buf, sizeof(buf), "%s%s:%s0x%02X", out.empty() ? "" : " ", ActionName(r.action),
                      r.modifiers == kChord ? "Ctrl+Shift+" : "", static_cast<unsigned>(r.vk));
        out += buf;
    }
    return out;
}

// HotkeyPoller skips a key code of 0, so a nav key of 0 is not registered.
void AddNav(std::vector<Registration>& regs, Action action, int vk) {
    if (vk != 0) regs.push_back({action, vk, kNav});
}

// Hotkeys::Start at v0.3.1 (src/hotkeys.cpp), with the poller calls recorded.
std::vector<Registration> OracleHotkeys(const oracle_api::Config& c) {
    std::vector<Registration> regs;
    AddNav(regs, Action::Toggle, c.vk_toggle);
    AddNav(regs, Action::CycleMode, c.vk_cycle_mode);
    AddNav(regs, Action::YawMode, c.vk_yaw_mode);
    AddNav(regs, Action::AdsMode, c.vk_ads_mode);
    if (c.chord_toggle) regs.push_back({Action::Toggle, 'Y', kChord});
    if (c.chord_cycle_mode) regs.push_back({Action::CycleMode, 'G', kChord});
    if (c.chord_yaw_mode) regs.push_back({Action::YawMode, 'H', kChord});
    if (c.chord_ads_mode) regs.push_back({Action::AdsMode, 'U', kChord});
    std::sort(regs.begin(), regs.end());
    return regs;
}

// Hotkeys::Start as the build that carries the frozen reader runs it (src/hotkeys.cpp at
// 1a266c2), with the poller calls recorded.
std::vector<Registration> ImportHotkeys(const legacy::Config& c) {
    std::vector<Registration> regs;
    AddNav(regs, Action::Toggle, c.vk_toggle);
    AddNav(regs, Action::CycleMode, c.vk_cycle_mode);
    AddNav(regs, Action::YawMode, c.vk_yaw_mode);
    if (c.chord_toggle) regs.push_back({Action::Toggle, 'Y', kChord});
    if (c.chord_cycle_mode) regs.push_back({Action::CycleMode, 'G', kChord});
    if (c.chord_yaw_mode) regs.push_back({Action::YawMode, 'H', kChord});
    std::sort(regs.begin(), regs.end());
    return regs;
}

uint32_t Bits(float f) {
    uint32_t b;
    std::memcpy(&b, &f, sizeof(b));
    return b;
}

// Every field the two Configs share, floats bit for bit. The oracle's ADS fields have no
// counterpart; comparison 1 lists them. Both builds' TrackingRuntime::Start takes the whole
// of its startup state from these fields, so equal fields are an equal start.
template <class A, class B>
std::vector<std::string> SharedFieldDifferences(const A& a, const B& b) {
    std::vector<std::string> out;
    const auto check = [&out](bool same, const char* name) {
        if (!same) out.push_back(name);
    };
#define SAME(f) check(a.f == b.f, #f)
#define SAME_BITS(f) check(Bits(a.f) == Bits(b.f), #f)
    SAME(enabled_on_startup);
    SAME(udp_port);
    SAME_BITS(sens_yaw);
    SAME_BITS(sens_pitch);
    SAME_BITS(sens_roll);
    SAME(invert_yaw);
    SAME(invert_pitch);
    SAME(invert_roll);
    SAME_BITS(local_smoothing);
    SAME_BITS(remote_smoothing);
    SAME_BITS(deadzone_deg);
    SAME(data_freshness_ms);
    SAME(world_space_yaw);
    SAME(position_enabled);
    SAME_BITS(pos_sens_x);
    SAME_BITS(pos_sens_y);
    SAME_BITS(pos_sens_z);
    SAME_BITS(pos_limit_x);
    SAME_BITS(pos_limit_y);
    SAME_BITS(pos_limit_z);
    SAME_BITS(pos_limit_z_back);
    SAME(invert_pos_x);
    SAME(invert_pos_y);
    SAME(invert_pos_z);
    SAME_BITS(position_scale);
    SAME(vk_toggle);
    SAME(vk_cycle_mode);
    SAME(vk_yaw_mode);
    SAME(chord_toggle);
    SAME(chord_cycle_mode);
    SAME(chord_yaw_mode);
#undef SAME
#undef SAME_BITS
    return out;
}

// ---------------------------------------------------------------------------
// Comparison 1: v0.3.1 against the frozen reader
// ---------------------------------------------------------------------------

// What a player updating from v0.3.1 sees change, and the commit that made each change.
// The changelog carries the same list.
struct ListedDifference {
    const char* id;
    const char* commit;
    const char* what;
    int seen = 0;
};

ListedDifference kComparisonOneDifferences[] = {
    {"ads-mode", "1dba8be",
     "[General] AdsMode is no longer read: head tracking carries on through the zoom in every case, "
     "and the lean eases out while zoomed"},
    {"ads-key", "1dba8be",
     "[Hotkeys] AdsMode and ChordAdsMode are no longer read, and neither Insert nor Ctrl+Shift+U "
     "cycles an ADS mode"},
};

ListedDifference& Listed(const char* id) {
    for (ListedDifference& d : kComparisonOneDifferences) {
        if (std::strcmp(d.id, id) == 0) return d;
    }
    throw std::logic_error(id);
}

struct OracleRun {
    bool usable = false;
    oracle_api::Config cfg;
};

struct ImportRun {
    legacy::ReadStatus status = legacy::ReadStatus::Read;
    legacy::Config cfg;
};

bool ImportUsable(legacy::ReadStatus s) {
    return s == legacy::ReadStatus::Read || s == legacy::ReadStatus::Absent;
}

void CompareOracleWithImport(const std::string& name, const OracleRun& o, const ImportRun& i) {
    if (o.usable != ImportUsable(i.status)) {
        Fail(name, std::string("v0.3.1 ") + (o.usable ? "starts" : "does not start") + ", the import " +
                       (ImportUsable(i.status) ? "starts" : "does not start"));
        return;
    }
    if (!o.usable) return;

    for (const std::string& field : SharedFieldDifferences(o.cfg, i.cfg)) {
        Fail(name, "comparison 1: " + field + " differs from v0.3.1 with no listed reason");
    }

    if (!o.cfg.ads_mode_is_default) ++Listed("ads-mode").seen;

    std::vector<Registration> expected;
    bool adsBound = false;
    for (const Registration& r : OracleHotkeys(o.cfg)) {
        if (r.action == Action::AdsMode) {
            adsBound = true;
        } else {
            expected.push_back(r);
        }
    }
    if (adsBound) ++Listed("ads-key").seen;
    const std::vector<Registration> actual = ImportHotkeys(i.cfg);
    if (expected != actual) {
        Fail(name, "comparison 1: hotkeys " + Describe(actual) + ", v0.3.1 less the listed differences " +
                       Describe(expected));
    }
}

// ---------------------------------------------------------------------------
// The keys the frozen reader reads, and the corpus descriptors
// ---------------------------------------------------------------------------

std::vector<MutationKey> CorpusKeys() {
    const auto boolean = [](const char* s, const char* k, const char* alternate) {
        return MutationKey{s, k, alternate, {}, false, {}};
    };
    const auto sens = [](const char* s, const char* k) { return MutationKey{s, k, "0.5", {}, false, {}}; };
    const auto limit = [](const char* k) { return MutationKey{"Position", k, "0.25", {"10.5", "-0.1"}, false, {}}; };
    const auto smooth = [](const char* k) { return MutationKey{"Smoothing", k, "0.3", {"1.5", "-0.5"}, false, {}}; };
    const auto hotkey = [](const char* k, const char* alt, const char* chord) {
        return MutationKey{"Hotkeys", k, alt, {"0x100", "-1"}, true, {ChordSwitch{"Hotkeys", chord, "1", "0"}}};
    };
    return {
        boolean("General", "EnableOnStartup", "0"),
        MutationKey{"General", "Port", "5000", {"1023", "65536"}, false, {}},
        MutationKey{"General", "DataFreshnessMs", "250", {"0", "-5"}, false, {}},
        boolean("General", "WorldSpaceYaw", "0"),
        sens("Sensitivity", "Yaw"),
        sens("Sensitivity", "Pitch"),
        sens("Sensitivity", "Roll"),
        boolean("Sensitivity", "InvertYaw", "0"),
        boolean("Sensitivity", "InvertPitch", "1"),
        boolean("Sensitivity", "InvertRoll", "0"),
        smooth("LocalSmoothing"),
        smooth("RemoteSmoothing"),
        MutationKey{"Smoothing", "DeadzoneDeg", "0.5", {"-1"}, false, {}},
        boolean("Position", "Enabled", "0"),
        sens("Position", "SensitivityX"),
        sens("Position", "SensitivityY"),
        sens("Position", "SensitivityZ"),
        limit("LimitX"),
        limit("LimitY"),
        limit("LimitZ"),
        limit("LimitZBack"),
        MutationKey{"Position", "PositionScale", "16", {}, false, {}},
        boolean("Position", "InvertX", "1"),
        boolean("Position", "InvertY", "1"),
        boolean("Position", "InvertZ", "1"),
        hotkey("Toggle", "0x70", "ChordToggle"),
        hotkey("CycleMode", "0x71", "ChordCycleMode"),
        hotkey("YawMode", "0x72", "ChordYawMode"),
        boolean("Hotkeys", "ChordToggle", "0"),
        boolean("Hotkeys", "ChordCycleMode", "0"),
        boolean("Hotkeys", "ChordYawMode", "0"),
    };
}

// ---------------------------------------------------------------------------
// Comparison 2: the frozen reader against the migration
// ---------------------------------------------------------------------------

// What the mod starts with. Pose shaping is not here: the migrated build applies none, and
// CheckPoseShaping holds the import to listing every value it leaves out.
struct Startup {
    int port = 0;
    bool enabled = false;
    TrackingMode mode = TrackingMode::RotationAndPosition;
    bool world_yaw = false;
    int data_freshness_ms = 0;
    uint32_t local_smoothing = 0;
    uint32_t remote_smoothing = 0;
    uint32_t limit_x = 0;
    uint32_t limit_y = 0;
    uint32_t limit_y_down = 0;
    uint32_t limit_z = 0;
    uint32_t limit_z_back = 0;
    std::vector<Registration> hotkeys;
};

const cfg::DroppedValue* FindDrop(const std::vector<cfg::DroppedValue>& dropped, cfg::DropRule rule,
                                  const char* section, const char* key) {
    for (const cfg::DroppedValue& d : dropped) {
        if (d.rule == rule && d.section == section && d.key == key) return &d;
    }
    return nullptr;
}

// A limit that is not finite imports as its default (N2), and only then is it dropped.
uint32_t LimitAfterN2(const std::string& name, float value, float row_default, const char* key,
                      const std::vector<cfg::DroppedValue>& dropped) {
    const bool n2 = !std::isfinite(value);
    if (n2 != (FindDrop(dropped, cfg::DropRule::NonFiniteNumber, "Position", key) != nullptr)) {
        Fail(name, std::string("[Position] ") + key + " dropped as not finite does not match its value");
    }
    return Bits(n2 ? row_default : value);
}

// A hotkey code outside 0x01-0xFE imports as unbound (N1), and only then is it dropped. 0 was
// already unbound, and is not recorded.
bool KeyAfterN1(const std::string& name, int vk, const char* key, const std::vector<cfg::DroppedValue>& dropped) {
    const bool outOfRange = vk != 0 && (vk < 0x01 || vk > 0xFE);
    if (outOfRange != (FindDrop(dropped, cfg::DropRule::KeyCodeOutOfRange, "Hotkeys", key) != nullptr)) {
        Fail(name, std::string("[Hotkeys] ") + key + " dropped as out of range does not match its code");
    }
    return !outOfRange;
}

// The frozen reader's build with the approved rules applied: TrackingRuntime::Start at 1a266c2,
// where [Position] Enabled chose between the first two modes and LimitY bounded both
// directions, and the keys Hotkeys::Start registered, less a code N1 unbinds.
Startup FromImport(const std::string& name, const legacy::Config& c, const std::vector<cfg::DroppedValue>& dropped) {
    const Config defaults = RedEclipseHeadTracking::MakeConfigTable().defaults();
    Startup s;
    s.port = c.udp_port;
    s.enabled = c.enabled_on_startup;
    s.mode = c.position_enabled ? TrackingMode::RotationAndPosition : TrackingMode::RotationOnly;
    s.world_yaw = c.world_space_yaw;
    s.data_freshness_ms = c.data_freshness_ms;
    s.local_smoothing = Bits(c.local_smoothing);
    s.remote_smoothing = Bits(c.remote_smoothing);
    s.limit_x = LimitAfterN2(name, c.pos_limit_x, defaults.position.limit_x, "LimitX", dropped);
    s.limit_y = LimitAfterN2(name, c.pos_limit_y, defaults.position.limit_y, "LimitY", dropped);
    s.limit_y_down = s.limit_y;
    s.limit_z = LimitAfterN2(name, c.pos_limit_z, defaults.position.limit_z, "LimitZ", dropped);
    s.limit_z_back = LimitAfterN2(name, c.pos_limit_z_back, defaults.position.limit_z_back, "LimitZBack", dropped);
    const bool toggleKept = KeyAfterN1(name, c.vk_toggle, "Toggle", dropped);
    const bool cycleKept = KeyAfterN1(name, c.vk_cycle_mode, "CycleMode", dropped);
    const bool yawKept = KeyAfterN1(name, c.vk_yaw_mode, "YawMode", dropped);
    for (const Registration& r : ImportHotkeys(c)) {
        const bool kept = r.modifiers == kChord || (r.action == Action::Toggle && toggleKept) ||
                          (r.action == Action::CycleMode && cycleKept) || (r.action == Action::YawMode && yawKept);
        if (kept) s.hotkeys.push_back(r);
    }
    return s;
}

// This build: TrackingRuntime::Start, and the lists Hotkeys::Start registers.
Startup FromMigration(const Config& c) {
    Startup s;
    s.port = c.udp_port;
    s.enabled = c.enable_on_startup;
    s.mode = cameraunlock::DecodeTrackingMode(c.rotation_enabled, c.position_enabled).value();
    s.world_yaw = c.world_space_yaw;
    s.data_freshness_ms = c.data_freshness_ms;
    s.local_smoothing = Bits(c.local_smoothing);
    s.remote_smoothing = Bits(c.remote_smoothing);
    s.limit_x = Bits(c.position.limit_x);
    s.limit_y = Bits(c.position.limit_y);
    s.limit_y_down = Bits(c.position.limit_y_down);
    s.limit_z = Bits(c.position.limit_z);
    s.limit_z_back = Bits(c.position.limit_z_back);
    const std::pair<Action, const std::string*> lists[] = {
        {Action::Toggle, &c.toggle_key_name},
        {Action::CycleMode, &c.cycle_tracking_mode_key_name},
        {Action::YawMode, &c.yaw_mode_key_name},
    };
    for (const auto& [action, list] : lists) {
        const cameraunlock::input::KeyBindingsParseResult parsed = cameraunlock::input::ParseKeyBindings(*list);
        if (!parsed.ok()) throw std::logic_error("a migrated hotkey list does not parse: " + *list);
        for (const cameraunlock::input::KeyBinding& b : parsed.bindings) {
            s.hotkeys.push_back({action, b.vk, static_cast<unsigned>(b.modifiers)});
        }
    }
    std::sort(s.hotkeys.begin(), s.hotkeys.end());
    return s;
}

std::vector<std::string> StartupDifferences(const Startup& a, const Startup& b) {
    std::vector<std::string> out;
#define SAME(f) \
    if (a.f != b.f) out.push_back(#f)
    SAME(port);
    SAME(enabled);
    SAME(mode);
    SAME(world_yaw);
    SAME(data_freshness_ms);
    SAME(local_smoothing);
    SAME(remote_smoothing);
    SAME(limit_x);
    SAME(limit_y);
    SAME(limit_y_down);
    SAME(limit_z);
    SAME(limit_z_back);
#undef SAME
    if (a.hotkeys != b.hotkeys) out.push_back("hotkeys " + Describe(a.hotkeys) + " against " + Describe(b.hotkeys));
    return out;
}

// Every sensitivity, inversion, deadzone and unit scale the frozen reader read is listed in its
// place, folded where it holds the value every build shipped and dropped as PoseShaping where it
// does not. Returns how many were dropped.
int CheckPoseShaping(const std::string& name, const legacy::Config& c, const cfg::ImportResult& result) {
    struct Read {
        const char* section;
        const char* key;
        bool shipped;
    };
    const Read reads[] = {
        {"Sensitivity", "Yaw", c.sens_yaw == legacy::kDefaultSensitivity},
        {"Sensitivity", "Pitch", c.sens_pitch == legacy::kDefaultSensitivity},
        {"Sensitivity", "Roll", c.sens_roll == legacy::kDefaultSensitivity},
        {"Sensitivity", "InvertYaw", c.invert_yaw == legacy::kDefaultInvertYaw},
        {"Sensitivity", "InvertPitch", c.invert_pitch == legacy::kDefaultInvert},
        {"Sensitivity", "InvertRoll", c.invert_roll == legacy::kDefaultInvertRoll},
        {"Smoothing", "DeadzoneDeg", c.deadzone_deg == legacy::kDefaultDeadzoneDeg},
        {"Position", "SensitivityX", c.pos_sens_x == legacy::kDefaultPosSens},
        {"Position", "SensitivityY", c.pos_sens_y == legacy::kDefaultPosSens},
        {"Position", "SensitivityZ", c.pos_sens_z == legacy::kDefaultPosSens},
        {"Position", "PositionScale", c.position_scale == legacy::kDefaultPositionScale},
        {"Position", "InvertX", c.invert_pos_x == legacy::kDefaultInvertPosX},
        {"Position", "InvertY", c.invert_pos_y == legacy::kDefaultInvert},
        {"Position", "InvertZ", c.invert_pos_z == legacy::kDefaultInvertPosZ},
    };
    if (result.pose_shaping.size() != std::size(reads)) {
        Fail(name, "the import lists " + std::to_string(result.pose_shaping.size()) + " pose-shaping values, not 14");
        return 0;
    }
    int dropped = 0;
    for (size_t k = 0; k < std::size(reads); ++k) {
        const cfg::PoseShapingValue& v = result.pose_shaping[k];
        const std::string label = std::string("[") + reads[k].section + "] " + reads[k].key;
        if (v.section != reads[k].section || v.key != reads[k].key) Fail(name, label + " is not listed in its place");
        if (v.folded != reads[k].shipped) Fail(name, label + " is " + (v.folded ? "folded" : "dropped") + " wrongly");
        const bool listed = FindDrop(result.dropped, cfg::DropRule::PoseShaping, reads[k].section, reads[k].key) != nullptr;
        if (listed == reads[k].shipped) {
            Fail(name, label + (listed ? " is dropped at its shipped value" : " is changed and not dropped"));
        }
        if (!reads[k].shipped) ++dropped;
    }
    return dropped;
}

// Every drop the import recorded is by one of the approved rules this map applies.
void CheckDropRules(const std::string& name, const cfg::ImportResult& result) {
    for (const cfg::DroppedValue& d : result.dropped) {
        const bool approved = d.rule == cfg::DropRule::PoseShaping || d.rule == cfg::DropRule::NonFiniteNumber ||
                              d.rule == cfg::DropRule::KeyCodeOutOfRange;
        if (!approved) Fail(name, "the import drops [" + d.section + "] " + d.key + " by a rule this map never applies");
    }
}

// A value the frozen reader accepts that the canonical row cannot hold, and that no approved
// rule covers. The owner defers such a file: it stays as it is, nothing is saved, and the
// session runs on what the import gave.
const char* const kUnrepresentable =
    "a DataFreshnessMs below 1, or a finite position limit below 0 or above 10, which the canonical rows "
    "cannot hold, so the import defers";

bool Unrepresentable(const legacy::Config& c) {
    const auto outside = [](float v) { return std::isfinite(v) && (v < 0.0f || v > 10.0f); };
    return c.data_freshness_ms < 1 || outside(c.pos_limit_x) || outside(c.pos_limit_y) || outside(c.pos_limit_z) ||
           outside(c.pos_limit_z_back);
}

// One run of comparison 2, over one Defaults.ini.
struct RunTally {
    int created = 0;
    int imported = 0;
    int deferred = 0;
    int refused = 0;
    // Migrated files holding at least one default row.
    int with_default_rows = 0;
    // Migrated files holding a value on at least one row, which the committed file never does.
    int with_values = 0;
};

struct MigrationTally {
    std::string committed;
    std::set<std::string> migrated;
    RunTally builtin;
    RunTally altered;
    int with_pose_shaping_dropped = 0;
    int with_n1 = 0;
    int with_n2 = 0;
};

// Every field the table binds, as the canonical renderer writes it, so two Configs compare whole.
std::string AllValues(const Config& c) {
    return cfg::RenderCanonical(RedEclipseHeadTracking::MakeConfigTable(), c,
                                {RedEclipseHeadTracking::kConfigDisplayName});
}

bool Contains(const std::vector<std::string>& lines, const std::string& text) {
    for (const std::string& line : lines) {
        if (line.find(text) != std::string::npos) return true;
    }
    return false;
}

// Where Defaults.ini is for each run of comparison 2: at the built-in values, which the first
// load creates, and with the values a player changed, written from it.
std::wstring g_builtinDefaults;
std::wstring g_alteredDefaults;

cfg::ConfigOwnerOptions<Config> OwnerOptions(const std::wstring& dir, const std::wstring& defaults) {
    return RedEclipseHeadTracking::MakeConfigOwnerOptions(dir + L"\\", cfg::DefaultsFile::At(defaults));
}

// ---------------------------------------------------------------------------
// The run
// ---------------------------------------------------------------------------

struct Folders {
    std::wstring oracle;
    std::wstring import;
    std::wstring migration;
};

const wchar_t kLegacyName[] = L"RedEclipseHeadTracking.ini";
const wchar_t kConfigName[] = L"CameraUnlock.ini";

using Listing = std::map<std::wstring, std::string>;

// The legacy file alone, holding `bytes`: nothing was imported.
Listing LegacyOnly(const std::string& bytes) {
    return Listing{{kLegacyName, bytes}};
}

void MigrateInput(const Folders& f, const std::string& input, const std::optional<std::string>& bytes,
                  const ImportRun& i, const cfg::ImportResult* result, MigrationTally& tally,
                  const std::wstring& defaults) {
    using cfg::ConfigLoadStatus;
    const bool builtin = defaults == g_builtinDefaults;
    RunTally& run = builtin ? tally.builtin : tally.altered;
    const std::string name = input + (builtin ? " (Defaults.ini at the built-in values)" : " (Defaults.ini changed)");

    EmptyFolder(f.migration);
    const std::wstring config = f.migration + L"\\" + kConfigName;
    const std::wstring legacy = f.migration + L"\\" + kLegacyName;
    const FileStamp defaultsBefore = Stamp(defaults);
    FileStamp legacyBefore;
    if (bytes) {
        WriteBytes(legacy, *bytes);
        legacyBefore = Stamp(legacy);
    }
    cfg::ConfigOwner<Config> owner(OwnerOptions(f.migration, defaults));
    const cfg::ConfigLoadResult<Config> loaded = owner.Load();
    const Listing after = Snapshot(f.migration);
    if (Stamp(defaults) != defaultsBefore) Fail(name, "the load wrote Defaults.ini");
    if (bytes && Stamp(legacy) != legacyBefore) {
        Fail(name, "RedEclipseHeadTracking.ini did not keep its bytes, write time and attributes");
    }

    if (!bytes) {
        // Not a migration: a fresh install, which follows Defaults.ini.
        ++run.created;
        if (loaded.status != ConfigLoadStatus::Created) Fail(name, "no file is not Created");
        if (after != Listing{{kConfigName, tally.committed}}) {
            Fail(name, "the folder does not hold CameraUnlock.ini as config/RedEclipseHeadTracking.ini and nothing else");
        }
        if (builtin) {
            for (const std::string& d : StartupDifferences(FromImport(name, i.cfg, {}), FromMigration(loaded.config))) {
                Fail(name, "comparison 2: " + d);
            }
        }
        return;
    }
    if (!ImportUsable(i.status)) {
        ++run.refused;
        if (loaded.status != ConfigLoadStatus::LegacyRefused) Fail(name, "a file the import refuses is not LegacyRefused");
        if (after != LegacyOnly(*bytes)) Fail(name, "a refused file got a CameraUnlock.ini or another file beside it");
        return;
    }

    if (builtin) {
        if (CheckPoseShaping(name, i.cfg, *result) > 0) ++tally.with_pose_shaping_dropped;
        CheckDropRules(name, *result);
        const auto hasRule = [result](cfg::DropRule rule) {
            return std::any_of(result->dropped.begin(), result->dropped.end(),
                               [rule](const cfg::DroppedValue& d) { return d.rule == rule; });
        };
        if (hasRule(cfg::DropRule::KeyCodeOutOfRange)) ++tally.with_n1;
        if (hasRule(cfg::DropRule::NonFiniteNumber)) ++tally.with_n2;
    }

    // Imported or deferred, the session runs on the settings the load hands back.
    for (const std::string& d :
         StartupDifferences(FromImport(name, i.cfg, result->dropped), FromMigration(loaded.config))) {
        Fail(name, "comparison 2: " + d);
    }

    if (Unrepresentable(i.cfg)) {
        ++run.deferred;
        if (loaded.status != ConfigLoadStatus::Deferred) {
            Fail(name, std::string(kUnrepresentable) + ", but the load is " + cfg::ConfigLoadStatusName(loaded.status));
        }
        if (after != LegacyOnly(*bytes)) Fail(name, "a deferred import created CameraUnlock.ini or another file");
        if (loaded.reason.find("cannot be converted") == std::string::npos) {
            Fail(name, "the player is not told which value stops the import: " + loaded.reason);
        }
        return;
    }

    ++run.imported;
    if (loaded.status != ConfigLoadStatus::Migrated) {
        Fail(name, std::string("the migration is ") + cfg::ConfigLoadStatusName(loaded.status) + ": " + loaded.reason);
        return;
    }
    const auto created = after.find(kConfigName);
    if (after.size() != 2 || created == after.end() || after.at(kLegacyName) != *bytes) {
        Fail(name, "the folder does not hold RedEclipseHeadTracking.ini and CameraUnlock.ini and nothing else");
        return;
    }
    if (!Contains(loaded.log, "created from")) Fail(name, "the log does not say where CameraUnlock.ini came from");
    const std::string& migrated = created->second;
    tally.migrated.insert(migrated);
    if (migrated.find("=default\r\n") != std::string::npos) ++run.with_default_rows;
    if (migrated != tally.committed) ++run.with_values;

    // The next launch reads CameraUnlock.ini over the same Defaults.ini, with nothing to report,
    // to the same settings, does not import, and writes neither file.
    cfg::ConfigOwner<Config> again(OwnerOptions(f.migration, defaults));
    const cfg::ConfigLoadResult<Config> reread = again.Load();
    if (reread.status != ConfigLoadStatus::Canonical || !reread.diagnostics.empty()) {
        Fail(name, "the next launch does not read CameraUnlock.ini cleanly");
    }
    if (AllValues(reread.config) != AllValues(loaded.config)) Fail(name, "the next launch runs on other settings");
    if (Contains(reread.log, "created from")) Fail(name, "the next launch imports again");
    if (!Contains(reread.log, "is left as it was and is not read")) {
        Fail(name, "the next launch does not say RedEclipseHeadTracking.ini is not read");
    }
    if (Snapshot(f.migration) != after || Stamp(legacy) != legacyBefore || Stamp(defaults) != defaultsBefore) {
        Fail(name, "the next launch changed a file");
    }

    // A read-only RedEclipseHeadTracking.ini imports as a writable one does and keeps its
    // attribute, bytes and write time.
    if (builtin) {
        EmptyFolder(f.migration);
        WriteBytes(legacy, *bytes);
        SetFileAttributesW(legacy.c_str(), FILE_ATTRIBUTE_READONLY);
        const FileStamp readOnlyBefore = Stamp(legacy);
        cfg::ConfigOwner<Config> readOnly(OwnerOptions(f.migration, defaults));
        const cfg::ConfigLoadResult<Config> fromReadOnly = readOnly.Load();
        if (fromReadOnly.status != ConfigLoadStatus::Migrated || AllValues(fromReadOnly.config) != AllValues(loaded.config) ||
            ReadBytes(config) != migrated) {
            Fail(name, "a read-only RedEclipseHeadTracking.ini does not import as a writable one does");
        }
        if (Stamp(legacy) != readOnlyBefore || (readOnlyBefore.attributes & FILE_ATTRIBUTE_READONLY) == 0) {
            Fail(name, "a read-only RedEclipseHeadTracking.ini did not keep its attribute, bytes and write time");
        }
    }
}

void RunInput(const Folders& f, const std::string& name, const std::optional<std::string>& bytes,
              MigrationTally& tally) {
    OracleRun o;
    {
        EmptyFolder(f.oracle);
        const std::wstring path = f.oracle + L"\\" + kLegacyName;
        if (bytes) WriteBytes(path, *bytes);
        o.usable = oracle_api::LoadOrCreate(Narrow(path).c_str(), o.cfg);
    }

    ImportRun i;
    std::optional<cfg::ImportResult> result;
    {
        EmptyFolder(f.import);
        const std::wstring path = f.import + L"\\" + kLegacyName;
        if (bytes) {
            WriteBytes(path, *bytes);
            SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_READONLY);
        }
        const auto before = Snapshot(f.import);
        i.status = legacy::Read(Narrow(path).c_str(), i.cfg);
        if (bytes) {
            Config mapped = RedEclipseHeadTracking::MakeConfigTable().defaults();
            result = RedEclipseHeadTracking::MakeLegacyImport().run(cfg::LegacyInput{path, Narrow(path), false}, mapped);
        }
        if (Snapshot(f.import) != before) Fail(name, "the import changed the folder it read from");
        if (!bytes && i.status != legacy::ReadStatus::Absent) Fail(name, "the import read a file that is not there");
    }

    CompareOracleWithImport(name, o, i);
    for (const std::wstring& defaults : {g_builtinDefaults, g_alteredDefaults}) {
        MigrateInput(f, name, bytes, i, result ? &*result : nullptr, tally, defaults);
    }
}

// Defaults.ini as a player may have changed it, from the one the owner created: every value
// this game reads differs from the built-in one, each set to the corpus's alternate for the
// key it comes from, so a corpus input holding that alternate migrates as default.
void WriteAlteredDefaults() {
    std::string text = ReadBytes(g_builtinDefaults);
    const std::pair<const char*, const char*> changes[] = {
        {"UdpPort=4242", "UdpPort=5000"},
        {"EnableOnStartup=true", "EnableOnStartup=false"},
        {"WorldSpaceYaw=true", "WorldSpaceYaw=false"},
        {"PositionEnabled=true", "PositionEnabled=false"},
        {"DataFreshnessMs=500", "DataFreshnessMs=250"},
        {"LocalSmoothing=0.0", "LocalSmoothing=0.3"},
        {"RemoteSmoothing=0.15", "RemoteSmoothing=0.3"},
        {"PositionLimitX=0.3", "PositionLimitX=0.25"},
        {"PositionLimitY=0.2", "PositionLimitY=0.25"},
        {"PositionLimitYDown=0.2", "PositionLimitYDown=0.25"},
        {"PositionLimitZ=0.4", "PositionLimitZ=0.25"},
        {"PositionLimitZBack=0.1", "PositionLimitZBack=0.25"},
        {"ToggleKey=End, Ctrl+Shift+Y", "ToggleKey=F1, Ctrl+Shift+Y"},
        {"CycleTrackingModeKey=PageUp, Ctrl+Shift+G", "CycleTrackingModeKey=F2, Ctrl+Shift+G"},
        {"YawModeKey=PageDown, Ctrl+Shift+H", "YawModeKey=F3, Ctrl+Shift+H"},
    };
    for (const auto& [from, to] : changes) {
        const std::string line = std::string("\r\n") + from + "\r\n";
        const size_t at = text.find(line);
        if (at == std::string::npos) throw std::runtime_error(std::string("the created Defaults.ini has no line ") + from);
        text.replace(at + 2, std::strlen(from), to);
    }
    const std::wstring folder = g_alteredDefaults.substr(0, g_alteredDefaults.find_last_of(L'\\'));
    if (!CreateDirectoryW(folder.c_str(), nullptr)) throw std::runtime_error("cannot create the changed Defaults.ini's folder");
    WriteBytes(g_alteredDefaults, text);
}

std::string ReadInput(const std::string& file) {
    return ReadBytes(Widen(std::string(RE_DIFFERENTIAL_INPUTS) + "/" + file));
}

std::string Replaced(std::string base, const std::string& from, const std::string& to) {
    const size_t at = base.find(from);
    if (at == std::string::npos) throw std::logic_error("no " + from + " in the base file");
    return base.replace(at, from.size(), to);
}

// Registration compares the two builds by key and modifiers, which holds only while a binding
// with no modifiers fires as the old build's NavGuarded did (not while Ctrl and Shift are both
// held) and a Ctrl+Shift binding as its ChordGuarded did (while both are held). Alt changes
// neither.
void TestRegistrationModel() {
    using cameraunlock::input::detail::BindingFires;
    for (unsigned held = 0; held < 8; ++held) {
        const auto mods = static_cast<KeyModifiers>(held);
        const bool chordHeld = cameraunlock::input::HasModifiers(mods, KeyModifiers::kCtrl | KeyModifiers::kShift);
        if (BindingFires(KeyModifiers::kNone, mods) != !chordHeld) {
            Fail("registration", "a key with no modifiers does not fire as NavGuarded did, held " + std::to_string(held));
        }
        if (BindingFires(KeyModifiers::kCtrl | KeyModifiers::kShift, mods) != chordHeld) {
            Fail("registration", "a Ctrl+Shift key does not fire as ChordGuarded did, held " + std::to_string(held));
        }
    }
}

void TestFrozenDefaults() {
    const oracle_api::Config o = oracle_api::Defaults();
    const legacy::Config l;
    for (const std::string& field : SharedFieldDifferences(o, l)) {
        Fail("defaults", field + ": the frozen default differs from v0.3.1's");
    }
}

}  // namespace

int main() {
    try {
        wchar_t temp[MAX_PATH];
        GetTempPathW(MAX_PATH, temp);
        const std::wstring root = std::wstring(temp) + L"red-eclipse-config-differential-" +
                                  std::to_wstring(GetCurrentProcessId());
        CreateDirectoryW(root.c_str(), nullptr);
        const Folders folders{MakeFolder(root, L"oracle"), MakeFolder(root, L"import"),
                              MakeFolder(root, L"migration")};
        MigrationTally tally;
        tally.committed = ReadBytes(Widen(RE_COMMITTED_CONFIG));

        // Each Defaults.ini sits outside the game folder, in a user folder of its own whose
        // parent exists, as the owner requires before it creates the file.
        const std::wstring builtinUser = MakeFolder(root, L"user-builtin");
        const std::wstring alteredUser = MakeFolder(root, L"user-altered");
        g_builtinDefaults = builtinUser + L"\\CameraUnlock\\Defaults.ini";
        g_alteredDefaults = alteredUser + L"\\CameraUnlock\\Defaults.ini";
        {
            EmptyFolder(folders.migration);
            cfg::ConfigOwner<Config> first(OwnerOptions(folders.migration, g_builtinDefaults));
            if (first.Load().status != cfg::ConfigLoadStatus::Created) Fail("Defaults.ini", "the first load is not Created");
            if (GetFileAttributesW(g_builtinDefaults.c_str()) == INVALID_FILE_ATTRIBUTES) {
                Fail("Defaults.ini", "the first load did not create Defaults.ini");
            }
        }
        WriteAlteredDefaults();

        TestFrozenDefaults();
        TestRegistrationModel();

        const std::string firstRunV020 = ReadInput("first-run-v0.2.0.ini");
        const std::string firstRunV030 = ReadInput("first-run-v0.3.0.ini");
        const std::string firstRun = ReadInput("first-run-v0.3.1.ini");
        {
            EmptyFolder(folders.oracle);
            const std::wstring path = folders.oracle + L"\\" + kLegacyName;
            oracle_api::Config created;
            if (!oracle_api::LoadOrCreate(Narrow(path).c_str(), created) || ReadBytes(path) != firstRun) {
                Fail("first run", "the oracle's first-run output is not inputs/first-run-v0.3.1.ini");
            }
        }

        const std::vector<std::pair<std::string, std::optional<std::string>>> inputs = {
            {"no file", std::nullopt},
            {"empty file", std::string()},
            {"first run, v0.2.0", firstRunV020},
            {"first run, v0.3.0", firstRunV030},
            {"first run, v0.3.1", firstRun},
            {"v0.3.1 file after its ADS cycle saved tracked", Replaced(firstRun, "AdsMode=paused", "AdsMode=tracked")},
            {"[Hotkeys] AdsMode on the YawMode key", Replaced(firstRun, "AdsMode=0x2D", "AdsMode=0x22")},
        };
        for (const auto& [name, bytes] : inputs) RunInput(folders, name, bytes, tally);

        // Fresh equals upgrade: over Defaults.ini at the built-in values, each published
        // build's first-run output, as RedEclipseHeadTracking.ini, imports into a
        // CameraUnlock.ini that is the committed file, which is what a fresh install creates.
        for (const std::string& file : {firstRunV020, firstRunV030, firstRun}) {
            EmptyFolder(folders.migration);
            WriteBytes(folders.migration + L"\\" + kLegacyName, file);
            cfg::ConfigOwner<Config> owner(OwnerOptions(folders.migration, g_builtinDefaults));
            if (owner.Load().status != cfg::ConfigLoadStatus::Migrated ||
                ReadBytes(folders.migration + L"\\" + kConfigName) != tally.committed) {
                Fail("first run", "a first-run file does not import into the committed file");
            }
        }

        const std::vector<IniMutation> corpus =
            GenerateIniMutations(firstRun, legacy::ReadKeys(), CorpusKeys());
        for (const IniMutation& m : corpus) RunInput(folders, "corpus: " + m.name, m.bytes, tally);

        std::printf("%zu inputs, %zu of them from the corpus\n", inputs.size() + corpus.size(), corpus.size());
        std::printf("comparison 1, v0.3.1 against the frozen reader:\n");
        for (const ListedDifference& d : kComparisonOneDifferences) {
            std::printf("  %s (%s): %d inputs\n    %s\n", d.id, d.commit, d.seen, d.what);
            if (d.seen == 0) Fail(d.id, "a listed difference no input shows");
        }
        std::printf("comparison 2, the frozen reader against the migration, %zu distinct files:\n",
                    tally.migrated.size());
        for (const auto& [over, run] : {std::pair<const char*, const RunTally*>{"at the built-in values", &tally.builtin},
                                        std::pair<const char*, const RunTally*>{"changed", &tally.altered}}) {
            std::printf("  over Defaults.ini %s: %d created, %d imported (%d holding a default row, %d a value), "
                        "%d deferred, %d refused as v0.3.1 refused them\n",
                        over, run->created, run->imported, run->with_default_rows, run->with_values, run->deferred,
                        run->refused);
            if (run->deferred == 0) Fail("deferral", std::string("no input is deferred over ") + over);
            if (run->with_default_rows == 0) Fail("default", std::string("no import writes default over ") + over);
            if (run->with_values == 0) Fail("default", std::string("no import writes a value over ") + over);
        }
        std::printf("  %d with a changed sensitivity, inversion, deadzone or scale dropped (pose_shaping)\n",
                    tally.with_pose_shaping_dropped);
        std::printf("  %d with a limit that is not a finite number set to its default (N2)\n", tally.with_n2);
        std::printf("  %d with a hotkey code outside 0x01-0xFE unbound (N1)\n", tally.with_n1);
        std::printf("  deferred: %s\n", kUnrepresentable);
        if (tally.with_pose_shaping_dropped == 0) Fail("pose shaping", "no input drops a changed value");
        if (tally.with_n1 == 0) Fail("N1", "no input unbinds an out-of-range hotkey code");
        if (tally.with_n2 == 0) Fail("N2", "no input sets a non-finite limit to its default");
        if (tally.migrated.count(tally.committed) == 0) Fail("first run", "no input migrated to the committed file");

        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::wstring lintDir(exe);
        lintDir = lintDir.substr(0, lintDir.find_last_of(L'\\'));
        lintDir = MakeFolder(lintDir, L"migrated");
        int n = 0;
        for (const std::string& file : tally.migrated) {
            WriteBytes(lintDir + L"\\" + std::to_wstring(n++) + L".ini", file);
        }

        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    } catch (const std::exception& e) {
        std::printf("FAIL: %s\n", e.what());
        return 1;
    }

    if (g_failures == 0) {
        std::printf("config differential: all passed\n");
        return 0;
    }
    std::printf("config differential: %d failure(s)\n", g_failures);
    return 1;
}
