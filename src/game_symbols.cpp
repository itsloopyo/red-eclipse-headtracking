#include "game_symbols.h"

#include "logging.h"

#include <dbghelp.h>

#include <string>

namespace RedEclipseHeadTracking {

namespace {

// DbgHelp keys its state on the "process handle" it is given, and it is not
// re-entrant for a single key. Red Eclipse links dbghelp itself for crash
// dumps, so the mod passes an arbitrary unique value instead of
// GetCurrentProcess(): the two symbol sessions then cannot disturb each other.
// The handle is never dereferenced because every module is loaded from a file
// path rather than read out of the live process.
const HANDLE kSymOwner = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x52454854));  // 'REHT'

struct EnumResult {
    DWORD64 address = 0;
    int hits = 0;
};

BOOL CALLBACK CollectSymbol(PSYMBOL_INFOW info, ULONG, PVOID ctx) {
    EnumResult* out = static_cast<EnumResult*>(ctx);
    out->address = info->Address;
    out->hits++;
    return TRUE;
}

// Resolves one symbol to its RVA. Masks must be fully qualified
// ("game::recomputecamera", not "recomputecamera") - DbgHelp matches the
// undecorated name including its namespace, and the bare form silently
// matches nothing. Requiring exactly one hit keeps an ambiguous name (there
// are both hud::hasinput and UI::hasinput) from resolving to the wrong one.
bool ResolveRva(DWORD64 pdbBase, const wchar_t* mask, uintptr_t& rvaOut) {
    EnumResult result;
    if (!SymEnumSymbolsW(kSymOwner, pdbBase, mask, CollectSymbol, &result)) {
        Log::Line("ERROR: symbol enumeration failed for '%ls' (win32 %lu)", mask, GetLastError());
        return false;
    }
    if (result.hits != 1) {
        Log::Line("ERROR: symbol '%ls' matched %d times, expected exactly 1", mask, result.hits);
        return false;
    }
    rvaOut = static_cast<uintptr_t>(result.address - pdbBase);
    return true;
}

std::wstring ParentDirectory(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return std::wstring();
    return path.substr(0, slash);
}

}  // namespace

bool GameSymbols::Resolve(HMODULE gameModule) {
    wchar_t exePath[MAX_PATH] = {};
    if (!GetModuleFileNameW(gameModule, exePath, MAX_PATH)) {
        Log::Line("ERROR: GetModuleFileNameW failed (win32 %lu)", GetLastError());
        return false;
    }

    // redeclipse.exe lives in <game root>\bin\amd64 while its PDB sits in the
    // game root, so both directories go on the symbol search path.
    std::wstring exeDir = ParentDirectory(exePath);
    std::wstring gameRoot = ParentDirectory(ParentDirectory(exeDir));
    std::wstring searchPath = exeDir + L";" + gameRoot;

    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_NO_PROMPTS | SYMOPT_EXACT_SYMBOLS);
    if (!SymInitializeW(kSymOwner, searchPath.c_str(), FALSE)) {
        Log::Line("ERROR: SymInitializeW failed (win32 %lu)", GetLastError());
        return false;
    }

    bool ok = false;
    DWORD64 pdbBase = SymLoadModuleExW(kSymOwner, nullptr, exePath, nullptr, 0, 0, nullptr, 0);
    if (!pdbBase) {
        Log::Line("ERROR: SymLoadModuleExW failed for %ls (win32 %lu)", exePath, GetLastError());
        SymCleanup(kSymOwner);
        return false;
    }

    IMAGEHLP_MODULEW64 moduleInfo;
    moduleInfo.SizeOfStruct = sizeof(moduleInfo);
    if (SymGetModuleInfoW64(kSymOwner, pdbBase, &moduleInfo) && moduleInfo.SymType == SymPdb) {
        Log::Line("Symbols: loaded %ls", moduleInfo.LoadedPdbName);

        // Every address the mod touches, resolved by name. Grouped only for
        // readability - a single missing entry aborts the whole resolve.
        struct Entry {
            const wchar_t* mask;
            uintptr_t* rva;
        };
        uintptr_t rvaSetCamMatrix = 0, rvaRecomputeCamera = 0, rvaDrawPointers = 0, rvaHasInput = 0;
        uintptr_t rvaCamera1 = 0, rvaCamera = 0, rvaCamMatrix = 0, rvaCamProjMatrix = 0;
        uintptr_t rvaCamDir = 0, rvaCamRight = 0, rvaCamUp = 0, rvaWorldPos = 0;

        const Entry entries[] = {
            {L"setcammatrix", &rvaSetCamMatrix},
            {L"game::recomputecamera", &rvaRecomputeCamera},
            {L"hud::drawpointers", &rvaDrawPointers},
            {L"hud::hasinput", &rvaHasInput},
            {L"camera1", &rvaCamera1},
            {L"camera", &rvaCamera},
            {L"cammatrix", &rvaCamMatrix},
            {L"camprojmatrix", &rvaCamProjMatrix},
            {L"camdir", &rvaCamDir},
            {L"camright", &rvaCamRight},
            {L"camup", &rvaCamUp},
            {L"worldpos", &rvaWorldPos},
        };

        ok = true;
        for (const Entry& entry : entries) {
            if (!ResolveRva(pdbBase, entry.mask, *entry.rva)) {
                ok = false;
                break;
            }
        }

        if (ok) {
            uintptr_t moduleBase = reinterpret_cast<uintptr_t>(gameModule);
            auto at = [moduleBase](uintptr_t rva) { return reinterpret_cast<void*>(moduleBase + rva); };

            setcammatrix = reinterpret_cast<void (*)()>(at(rvaSetCamMatrix));
            recomputecamera = reinterpret_cast<void (*)()>(at(rvaRecomputeCamera));
            drawpointers = reinterpret_cast<void (*)(int, int, float, float, float)>(at(rvaDrawPointers));
            hasinput = reinterpret_cast<int (*)(bool, bool)>(at(rvaHasInput));

            camera1 = static_cast<void**>(at(rvaCamera1));
            camera = at(rvaCamera);
            cammatrix = static_cast<EngMat4*>(at(rvaCamMatrix));
            camprojmatrix = static_cast<EngMat4*>(at(rvaCamProjMatrix));
            camdir = static_cast<EngVec*>(at(rvaCamDir));
            camright = static_cast<EngVec*>(at(rvaCamRight));
            camup = static_cast<EngVec*>(at(rvaCamUp));
            worldpos = static_cast<EngVec*>(at(rvaWorldPos));

            Log::Line("Symbols: module base 0x%llX, setcammatrix +0x%llX, recomputecamera +0x%llX, drawpointers +0x%llX",
                      static_cast<unsigned long long>(moduleBase),
                      static_cast<unsigned long long>(rvaSetCamMatrix),
                      static_cast<unsigned long long>(rvaRecomputeCamera),
                      static_cast<unsigned long long>(rvaDrawPointers));
        }
    } else {
        Log::Line("ERROR: no PDB for %ls. Red Eclipse ships redeclipse_windows_amd64.pdb in the "
                  "game folder; verify the game files in Steam to restore it.", exePath);
    }

    // The PDB is ~29 MB of process memory and every address is already
    // captured, so the symbol session is torn down rather than left resident.
    SymUnloadModule64(kSymOwner, pdbBase);
    SymCleanup(kSymOwner);
    return ok;
}

}  // namespace RedEclipseHeadTracking
