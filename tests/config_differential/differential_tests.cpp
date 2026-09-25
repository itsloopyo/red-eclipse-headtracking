// The config differential test. Every input is read three ways:
//
//   oracle     v0.3.1's reader (oracle/), the newest published build, and v0.3.1's startup code
//   import     the frozen reader in src/legacy_config/, and the startup code it ran under
//   migration  the config owner converting the file, then the canonical reader and table on
//              the result, and the startup code of this build
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
// file, the session runs on what the import gave, and kUnrepresentable names them.
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
    "cannot hold, so the conversion defers";

bool Unrepresentable(const legacy::Config& c) {
    const auto outside = [](float v) { return std::isfinite(v) && (v < 0.0f || v > 10.0f); };
    return c.data_freshness_ms < 1 || outside(c.pos_limit_x) || outside(c.pos_limit_y) || outside(c.pos_limit_z) ||
           outside(c.pos_limit_z_back);
}

struct MigrationTally {
    std::string committed;
    std::set<std::string> migrated;
    int created = 0;
    int converted = 0;
    int deferred = 0;
    int refused = 0;
    int with_pose_shaping_dropped = 0;
    int with_n1 = 0;
    int with_n2 = 0;
};

std::string Render(const Config& c) {
    return cfg::RenderCanonical(RedEclipseHeadTracking::MakeConfigTable(), c,
                                {RedEclipseHeadTracking::kConfigDisplayName});
}

// ---------------------------------------------------------------------------
// The run
// ---------------------------------------------------------------------------

struct Folders {
    std::wstring oracle;
    std::wstring import;
    std::wstring migration;
};

const wchar_t kIniName[] = L"RedEclipseHeadTracking.ini";

void MigrateInput(const Folders& f, const std::string& name, const std::optional<std::string>& bytes,
                  const ImportRun& i, const cfg::ImportResult* result, MigrationTally& tally) {
    using cfg::ConfigLoadStatus;
    EmptyFolder(f.migration);
    const std::wstring path = f.migration + L"\\" + kIniName;
    if (bytes) WriteBytes(path, *bytes);
    cfg::ConfigOwner<Config> owner(RedEclipseHeadTracking::MakeConfigOwnerOptions(path));
    const cfg::ConfigLoadResult<Config> loaded = owner.Load();
    const std::map<std::wstring, std::string> after = Snapshot(f.migration);

    if (!bytes) {
        ++tally.created;
        if (loaded.status != ConfigLoadStatus::Created) Fail(name, "no file is not Created");
        if (ReadBytes(path) != tally.committed) Fail(name, "the created file is not config/RedEclipseHeadTracking.ini");
        for (const std::string& d : StartupDifferences(FromImport(name, i.cfg, {}), FromMigration(loaded.config))) {
            Fail(name, "comparison 2: " + d);
        }
        return;
    }
    if (!ImportUsable(i.status)) {
        ++tally.refused;
        if (loaded.status != ConfigLoadStatus::LegacyRefused) Fail(name, "a file the import refuses is not LegacyRefused");
        if (after != std::map<std::wstring, std::string>{{kIniName, *bytes}}) {
            Fail(name, "a refused file did not keep its bytes, or got a copy");
        }
        return;
    }

    if (CheckPoseShaping(name, i.cfg, *result) > 0) ++tally.with_pose_shaping_dropped;
    CheckDropRules(name, *result);
    const auto hasRule = [result](cfg::DropRule rule) {
        return std::any_of(result->dropped.begin(), result->dropped.end(),
                           [rule](const cfg::DroppedValue& d) { return d.rule == rule; });
    };
    if (hasRule(cfg::DropRule::KeyCodeOutOfRange)) ++tally.with_n1;
    if (hasRule(cfg::DropRule::NonFiniteNumber)) ++tally.with_n2;

    // Converted or deferred, the session runs on the settings the load hands back.
    for (const std::string& d :
         StartupDifferences(FromImport(name, i.cfg, result->dropped), FromMigration(loaded.config))) {
        Fail(name, "comparison 2: " + d);
    }

    if (Unrepresentable(i.cfg)) {
        ++tally.deferred;
        if (loaded.status != ConfigLoadStatus::Deferred) {
            Fail(name, std::string(kUnrepresentable) + ", but the load is " + cfg::ConfigLoadStatusName(loaded.status));
        }
        if (after != std::map<std::wstring, std::string>{{kIniName, *bytes}}) {
            Fail(name, "a deferred file did not keep its bytes, or got a copy");
        }
        return;
    }

    ++tally.converted;
    if (loaded.status != ConfigLoadStatus::Migrated) {
        Fail(name, std::string("the migration is ") + cfg::ConfigLoadStatusName(loaded.status) + ": " + loaded.reason);
        return;
    }
    const auto copy = after.find(std::wstring(kIniName) + L".pre-canonical");
    if (copy == after.end() || copy->second != *bytes) Fail(name, ".pre-canonical is not the input");
    if (after.size() != 2) Fail(name, "the migration left files other than the config and its copy");

    const std::string migrated = ReadBytes(path);
    if (Render(loaded.config) != migrated) Fail(name, "rendering the re-read Config does not give the migrated bytes");
    tally.migrated.insert(migrated);

    cfg::ConfigOwner<Config> again(RedEclipseHeadTracking::MakeConfigOwnerOptions(path));
    const cfg::ConfigLoadResult<Config> reread = again.Load();
    if (reread.status != ConfigLoadStatus::Canonical || !reread.diagnostics.empty() ||
        Render(reread.config) != migrated || Snapshot(f.migration) != after || ReadBytes(path) != migrated) {
        Fail(name, "migrating the migrated file does something");
    }
}

void RunInput(const Folders& f, const std::string& name, const std::optional<std::string>& bytes,
              MigrationTally& tally) {
    OracleRun o;
    {
        EmptyFolder(f.oracle);
        const std::wstring path = f.oracle + L"\\" + kIniName;
        if (bytes) WriteBytes(path, *bytes);
        o.usable = oracle_api::LoadOrCreate(Narrow(path).c_str(), o.cfg);
    }

    ImportRun i;
    std::optional<cfg::ImportResult> result;
    {
        EmptyFolder(f.import);
        const std::wstring path = f.import + L"\\" + kIniName;
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
    MigrateInput(f, name, bytes, i, result ? &*result : nullptr, tally);
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

        TestFrozenDefaults();
        TestRegistrationModel();

        const std::string firstRunV020 = ReadInput("first-run-v0.2.0.ini");
        const std::string firstRunV030 = ReadInput("first-run-v0.3.0.ini");
        const std::string firstRun = ReadInput("first-run-v0.3.1.ini");
        {
            EmptyFolder(folders.oracle);
            const std::wstring path = folders.oracle + L"\\" + kIniName;
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

        // Fresh equals upgrade: each published build's first-run output converts to the
        // committed file, as no file is created as it.
        for (const std::string& file : {firstRunV020, firstRunV030, firstRun}) {
            EmptyFolder(folders.migration);
            const std::wstring path = folders.migration + L"\\" + kIniName;
            WriteBytes(path, file);
            cfg::ConfigOwner<Config> owner(RedEclipseHeadTracking::MakeConfigOwnerOptions(path));
            owner.Load();
            if (ReadBytes(path) != tally.committed) {
                Fail("first run", "a first-run file does not convert to the committed file");
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
        std::printf("comparison 2, the frozen reader against the migration: %d created, %d converted, "
                    "%d refused as v0.3.1 refused them, %zu distinct files\n",
                    tally.created, tally.converted, tally.refused, tally.migrated.size());
        std::printf("  %d with a changed sensitivity, inversion, deadzone or scale dropped (pose_shaping)\n",
                    tally.with_pose_shaping_dropped);
        std::printf("  %d with a limit that is not a finite number set to its default (N2)\n", tally.with_n2);
        std::printf("  %d with a hotkey code outside 0x01-0xFE unbound (N1)\n", tally.with_n1);
        std::printf("  %d deferred: %s\n", tally.deferred, kUnrepresentable);
        if (tally.with_pose_shaping_dropped == 0) Fail("pose shaping", "no input drops a changed value");
        if (tally.with_n1 == 0) Fail("N1", "no input unbinds an out-of-range hotkey code");
        if (tally.with_n2 == 0) Fail("N2", "no input sets a non-finite limit to its default");
        if (tally.deferred == 0) Fail("deferral", "no input is deferred");
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

        for (const std::wstring& dir : {folders.oracle, folders.import, folders.migration}) {
            EmptyFolder(dir);
            RemoveDirectoryW(dir.c_str());
        }
        RemoveDirectoryW(root.c_str());
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
