# Third-Party Notices

## Ultimate ASI Loader

- **Version:** v9.7.2 (x64)
- **License:** MIT
- **Upstream:** https://github.com/ThirteenAG/Ultimate-ASI-Loader
- **Usage:** Loads the `.asi` plugin from the game executable directory; installed as a `winmm.dll` proxy in `bin/amd64/` (redeclipse.exe imports `winmm.dll`, so that is the slot it loads ASI plugins through).
- **Bundled:** yes. Bundled in the release ZIP and used as the install-time source.

---

## MinHook

- **Version:** 1.3.3
- **License:** BSD-2-Clause
- **Upstream:** https://github.com/TsudaKageyu/minhook
- **Usage:** Function hooking primitives, compiled into the mod DLL and used to detour the engine's camera and crosshair functions.
- **Bundled:** yes. Statically compiled into the mod DLL shipped in the release ZIP.

---

## OpenTrack

- **Version:** N/A (wire protocol only)
- **License:** ISC
- **Upstream:** https://github.com/opentrack/opentrack
- **Usage:** UDP head-tracking wire protocol consumed by this mod. No OpenTrack code is bundled; only the protocol is implemented.
- **Bundled:** no.

---

## Game credit

Red Eclipse is a free and open-source game by the Red Eclipse Team, built on
the Cube 2 / Tesseract engine. It is distributed under the zlib licence with
its assets under various Creative Commons licences; see the game's own
`doc/` folder for the full terms.

This mod links against nothing from the game. It resolves the engine's own
function and variable addresses at runtime from the PDB that Red Eclipse ships
beside its executable, and no game code, assets, or binaries are included in
this repository or the release ZIPs.
