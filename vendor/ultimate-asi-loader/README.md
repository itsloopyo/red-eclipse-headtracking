# Ultimate ASI Loader (vendored)

Bundled copy of Ultimate ASI Loader (x64), the install-time source of truth.
Refresh manually with `pixi run update-deps`, then commit.

## Snapshot

- Upstream: https://github.com/ThirteenAG/Ultimate-ASI-Loader
- Tag: `v9.7.2`
- Commit: `ab722befd52581a34449b603926cfab476e66b05`
- Asset: `Ultimate-ASI-Loader_x64.zip`
- dinput8.dll SHA-256: `22fda9c71eaae02460f311bf3441638340ab591586d78f1de213c4819dcb883c`
- Fetched at: 2026-08-20T09:11:39.4372729+01:00

`dinput8.dll` is extracted from the upstream x64 zip untouched. install.cmd copies it
into the Red Eclipse bin/amd64/ directory as winmm.dll - redeclipse.exe imports
winmm.dll, so that is the proxy slot it loads ASI plugins through.
