# Ultimate ASI Loader (vendored)

Bundled copy of Ultimate ASI Loader for Red Eclipse, the install-time source of truth.
Refresh manually with `pixi run update-deps`, then commit.

## Snapshot

- Upstream: https://github.com/ThirteenAG/Ultimate-ASI-Loader
- Tag: `v9.7.2`
- Asset: `Ultimate-ASI-Loader_x64.zip`
- dinput8.dll SHA-256: `22fda9c71eaae02460f311bf3441638340ab591586d78f1de213c4819dcb883c`
- Fetched at: 2026-08-03T20:22:23.7032544+01:00

`dinput8.dll` is extracted from the upstream zip untouched. install.cmd copies it
to `bin/amd64/winmm.dll` (redeclipse.exe imports winmm.dll, so that is the proxy
slot it loads ASI plugins through).
