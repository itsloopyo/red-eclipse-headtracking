// The config differential test. Every input is read two ways:
//
//   oracle     v0.3.1's reader (oracle/), the newest published build, and v0.3.1's startup code
//   import     the frozen reader in src/legacy_config/, and the startup code it ran under
//
// Comparison 1, oracle against import, finds what a player updating from v0.3.1 sees change
// that the conversion did not cause. Every difference it may find is listed in
// kComparisonOneDifferences with the commit that made it; any other fails the test.
//
// Inputs: no file, an empty file, the first-run output of each published build (v0.2.0,
// v0.3.0 and v0.3.1, extracted once into inputs/; v0.1.0 was tagged but never released, and
// its reader and core pin are v0.2.0's), files v0.3.1 wrote after its ADS mode cycle saved,
// and core's corpus over v0.3.1's first-run output. No published build shipped a config file
// or a launcher seed: each created the file at first launch.

#include "legacy_config/legacy_config.h"
#include "oracle_adapter.h"

#include "cameraunlock/config/testing/ini_mutations.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

namespace legacy = RedEclipseHeadTracking::legacy;
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

constexpr unsigned kNav = 0;
constexpr unsigned kChord = 3;  // KeyModifiers::kCtrl | KeyModifiers::kShift

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
// The run
// ---------------------------------------------------------------------------

struct Folders {
    std::wstring oracle;
    std::wstring import;
};

const wchar_t kIniName[] = L"RedEclipseHeadTracking.ini";

void RunInput(const Folders& f, const std::string& name, const std::optional<std::string>& bytes) {
    OracleRun o;
    {
        EmptyFolder(f.oracle);
        const std::wstring path = f.oracle + L"\\" + kIniName;
        if (bytes) WriteBytes(path, *bytes);
        o.usable = oracle_api::LoadOrCreate(Narrow(path).c_str(), o.cfg);
    }

    ImportRun i;
    {
        EmptyFolder(f.import);
        const std::wstring path = f.import + L"\\" + kIniName;
        if (bytes) {
            WriteBytes(path, *bytes);
            SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_READONLY);
        }
        const auto before = Snapshot(f.import);
        i.status = legacy::Read(Narrow(path).c_str(), i.cfg);
        if (Snapshot(f.import) != before) Fail(name, "the import changed the folder it read from");
        if (!bytes && i.status != legacy::ReadStatus::Absent) Fail(name, "the import read a file that is not there");
    }

    CompareOracleWithImport(name, o, i);
}

std::string ReadInput(const std::string& file) {
    return ReadBytes(Widen(std::string(RE_DIFFERENTIAL_INPUTS) + "/" + file));
}

std::string Replaced(std::string base, const std::string& from, const std::string& to) {
    const size_t at = base.find(from);
    if (at == std::string::npos) throw std::logic_error("no " + from + " in the base file");
    return base.replace(at, from.size(), to);
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
        const Folders folders{MakeFolder(root, L"oracle"), MakeFolder(root, L"import")};

        TestFrozenDefaults();

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
        for (const auto& [name, bytes] : inputs) RunInput(folders, name, bytes);

        const std::vector<IniMutation> corpus =
            GenerateIniMutations(firstRun, legacy::ReadKeys(), CorpusKeys());
        for (const IniMutation& m : corpus) RunInput(folders, "corpus: " + m.name, m.bytes);

        std::printf("%zu inputs, %zu of them from the corpus\n", inputs.size() + corpus.size(), corpus.size());
        std::printf("comparison 1, v0.3.1 against the frozen reader:\n");
        for (const ListedDifference& d : kComparisonOneDifferences) {
            std::printf("  %s (%s): %d inputs\n    %s\n", d.id, d.commit, d.seen, d.what);
            if (d.seen == 0) Fail(d.id, "a listed difference no input shows");
        }

        for (const std::wstring& dir : {folders.oracle, folders.import}) {
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
