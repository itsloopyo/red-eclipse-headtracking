#include "camera_hook.h"
#include "config.h"
#include "game_symbols.h"
#include "hotkeys.h"
#include "logging.h"
#include "path_utils.h"
#include "tracking_runtime.h"

#include "cameraunlock/diagnostics/crash_handler.h"
#include "cameraunlock/memory/pe_fingerprint.h"

#include <windows.h>
#include <process.h>

#include <string>

namespace {

constexpr const char* kModName = "RedEclipseHeadTracking";
constexpr const char* kModVersion = "0.2.0";
constexpr const char* kGameExe = "redeclipse.exe";
constexpr int kInitMaxWaitMs = 30000;
constexpr int kInitPollMs = 100;
constexpr int kHeartbeatMs = 5000;

HANDLE g_initThreadHandle = nullptr;
volatile LONG g_shutdown = 0;

RedEclipseHeadTracking::GameSymbols g_symbols;
RedEclipseHeadTracking::TrackingRuntime g_tracking;
RedEclipseHeadTracking::Hotkeys g_hotkeys;

// Red Eclipse calls SteamAPI_RestartAppIfNecessary during startup
// (engine/cdpi.cpp). Started outside the Steam client it hands off to Steam,
// which launches a second copy, and this first process exits a couple of
// seconds later. Both copies load the ASI, so without this check the doomed
// one wins the race for the log file - it holds an exclusive write handle,
// the real game process cannot open the log, and the mod runs the whole
// session silently with a log that looks like it stopped at startup.
//
// Steam puts these variables in the environment of the process it launches,
// and a steam_appid.txt beside the executable suppresses the relaunch the same
// way. Either one means this process is the one that keeps running.
bool WillRelaunchThroughSteam() {
    if (GetEnvironmentVariableW(L"SteamAppId", nullptr, 0) != 0) return false;
    if (GetEnvironmentVariableW(L"SteamGameId", nullptr, 0) != 0) return false;

    wchar_t exePath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH)) {
        std::wstring path(exePath);
        size_t slash = path.find_last_of(L"\\/");
        if (slash != std::wstring::npos) {
            path.replace(slash + 1, std::wstring::npos, L"steam_appid.txt");
            if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) return false;
        }
    }
    return true;
}

void LogFingerprint(HMODULE gameModule) {
    using namespace RedEclipseHeadTracking;
    cameraunlock::memory::PeFingerprint fp{};
    if (cameraunlock::memory::ReadPeFingerprint(gameModule, fp)) {
        Log::Line("PE fingerprint: TimeDateStamp=0x%08X SizeOfImage=0x%08X CheckSum=0x%08X",
                  fp.TimeDateStamp, fp.SizeOfImage, fp.CheckSum);
    } else {
        Log::Line("WARN: could not read PE fingerprint of %s", kGameExe);
    }
}

unsigned __stdcall InitThread(void*) {
    using namespace RedEclipseHeadTracking;

    // Leave the about-to-be-replaced process completely alone: no log, no
    // symbol load, no hooks. The copy Steam launches does all of it.
    if (WillRelaunchThroughSteam()) {
        return 0;
    }

    // The ASI loads before the game has finished initialising; wait for the
    // executable image to be present before touching anything in it.
    HMODULE gameModule = nullptr;
    int waited = 0;
    while ((gameModule = GetModuleHandleA(kGameExe)) == nullptr) {
        Sleep(kInitPollMs);
        waited += kInitPollMs;
        if (waited >= kInitMaxWaitMs) {
            return 1;
        }
    }

    const std::wstring logPath = GetModulePathW("RedEclipseHeadTracking.log");
    // Keep one previous generation. The crash handler below writes its report
    // into this log, and the user relaunches before sending it - a plain
    // truncate would erase the very report we installed the handler for.
    MoveFileExW(logPath.c_str(), GetModulePathW("RedEclipseHeadTracking.prev.log").c_str(),
                MOVEFILE_REPLACE_EXISTING);
    Log::Open(logPath);
    cameraunlock::diagnostics::InstallCrashHandler();

    Log::Line("%s v%s attached to %s", kModName, kModVersion, kGameExe);
    LogFingerprint(gameModule);

    Config cfg;
    std::string iniPath = GetModulePath("RedEclipseHeadTracking.ini");
    if (!cfg.LoadOrCreate(iniPath.c_str())) {
        Log::Line("ERROR: Config load failed");
        return 1;
    }
    Log::Line("Config: port=%u enabled=%d localSmoothing=%.2f remoteSmoothing=%.2f sens=(%.2f,%.2f,%.2f)",
              cfg.udp_port, cfg.enabled_on_startup ? 1 : 0,
              cfg.local_smoothing, cfg.remote_smoothing,
              cfg.sens_yaw, cfg.sens_pitch, cfg.sens_roll);

    // Every address the mod uses comes from the PDB Red Eclipse ships beside
    // its executable. If that resolve fails there is nothing safe to hook, so
    // the mod stays dormant and the game runs exactly vanilla.
    if (!g_symbols.Resolve(gameModule)) {
        Log::Line("Staying dormant: game symbols could not be resolved. No hooks installed.");
        return 0;
    }

    if (!g_tracking.Start(cfg)) {
        Log::Line("ERROR: Tracking runtime start failed");
        return 1;
    }

    if (!g_hotkeys.Start(cfg,
                         [] { g_tracking.ToggleEnabled(); },
                         [] { g_tracking.CycleTrackingMode(); },
                         [] { g_tracking.ToggleYawMode(); })) {
        Log::Line("ERROR: Hotkeys start failed");
        g_tracking.Stop();
        return 1;
    }

    if (!InstallCameraHook(g_symbols, g_tracking, cfg)) {
        Log::Line("ERROR: Camera hook installation failed");
        g_hotkeys.Stop();
        g_tracking.Stop();
        return 1;
    }

    Log::Line("%s ready", kModName);

    bool lastReceiving = false;
    bool firstReport = true;
    while (InterlockedCompareExchange(&g_shutdown, 0, 0) == 0) {
        bool receiving = g_tracking.IsReceiving();
        if (firstReport || receiving != lastReceiving) {
            Log::Line("OpenTrack: %s", receiving ? "receiving data" : "no data");
            lastReceiving = receiving;
            firstReport = false;
        }
        Sleep(kHeartbeatMs);
    }

    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            g_initThreadHandle = reinterpret_cast<HANDLE>(
                _beginthreadex(nullptr, 0, InitThread, nullptr, 0, nullptr));
            break;

        case DLL_PROCESS_DETACH:
            InterlockedExchange(&g_shutdown, 1);
            if (g_initThreadHandle) {
                WaitForSingleObject(g_initThreadHandle, 2000);
                CloseHandle(g_initThreadHandle);
                g_initThreadHandle = nullptr;
            }
            RedEclipseHeadTracking::RemoveCameraHook();
            g_hotkeys.Stop();
            g_tracking.Stop();
            RedEclipseHeadTracking::Log::Close();
            break;
    }
    return TRUE;
}
