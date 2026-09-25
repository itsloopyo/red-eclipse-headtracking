# Changelog

## [Unreleased]

### Changed

- `RedEclipseHeadTracking.ini` has a new layout. The first time this version starts, it converts the file once into the new layout and keeps the file as it was beside it as `RedEclipseHeadTracking.ini.pre-canonical`. `RedEclipseHeadTracking.ini.pre-canonical.last`, when present, is the file as it was before the most recent conversion: the mod converts the file again when it finds the older layout later, for example after an older version of the mod rewrote it.
- Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:
  - A sensitivity, scale, deadzone, response curve or axis inversion you changed from its default. Set these in your tracker instead.
- Hotkeys are written as key names, and each hotkey lists every key that triggers it, the Ctrl+Shift chord included: `ToggleKey=End, Ctrl+Shift+Y`. The conversion carries over each key you had bound and each chord you had switched on or off, and now each one can be changed or removed like any other key.
- Settings moved: `[General] Port` is `[Network] UdpPort`, the `[Position]` limits are `PositionLimitX`, `PositionLimitY`, `PositionLimitYDown`, `PositionLimitZ` and `PositionLimitZBack`, and `[Hotkeys] Toggle`, `CycleMode` and `YawMode` with `ChordToggle`, `ChordCycleMode` and `ChordYawMode` are `ToggleKey`, `CycleTrackingModeKey` and `YawModeKey`. `LimitY` bounded both directions, so it becomes both `PositionLimitY` and `PositionLimitYDown`, which can now be set apart. `[Position] Enabled` chose the tracking mode at startup; that is now the pair `RotationEnabled` and `PositionEnabled`. The conversion carries every one of these values over.
- The tracking mode that Page Up or Ctrl+Shift+G selects, and the yaw mode that Page Down or Ctrl+Shift+H selects, are now saved as soon as you change them and come back at the next start. End still changes the current session only.
- An older version of the mod may not read the new layout correctly. It reads a key that moved as its own default, and it can misread a hotkey or another value that is now written as a name. To go back to an older version, first copy `RedEclipseHeadTracking.ini.pre-canonical` back over `RedEclipseHeadTracking.ini`, which restores the old file.
- `uninstall.cmd` keeps `bin\amd64\RedEclipseHeadTracking.ini` and its `.pre-canonical` copies, so your settings survive a reinstall.
- A position limit that is not a finite number (`nan`, `inf`) is converted to its default, and a hotkey code outside `0x01` to `0xFE` is not carried over, which leaves that key unbound. The log names each one.
- A file whose `DataFreshnessMs` is below 1, or whose position limits include one below 0 or above 10, is not converted, because the new layout cannot hold those values. The mod runs on the file's values with the same exceptions as a converted file: a sensitivity, scale, deadzone or inversion you changed is not applied, a limit that is not a finite number is its default, and an out-of-range hotkey code leaves that key unbound. It saves nothing that session, and says so in the log at every start until the value is fixed.
- When there is no `RedEclipseHeadTracking.ini` and the mod cannot create one because `bin\amd64` cannot be written, the mod now starts on its default settings and saves nothing that session. Earlier versions did not start at all in that case.
- Since v0.3.1, `[General] AdsMode`, `[Hotkeys] AdsMode` and `[Hotkeys] ChordAdsMode` are no longer read, and neither Insert nor Ctrl+Shift+U cycles an ADS mode: head tracking carries on through the zoom in every case, and the lean eases out while zoomed (1dba8be).

### Removed

- The sensitivity, scale, deadzone and axis inversion settings: `[Sensitivity] Yaw`, `Pitch`, `Roll`, `InvertYaw`, `InvertPitch` and `InvertRoll`, `[Smoothing] DeadzoneDeg`, and `[Position] SensitivityX`, `SensitivityY`, `SensitivityZ`, `PositionScale`, `InvertX`, `InvertY` and `InvertZ`. Set these in your tracker app instead. The yaw and roll inversions and the 8 world units per metre the mod shipped with are now part of its own axis conversion, so with these settings at their shipped values the camera moves as it did before.

## [0.3.1] - 2026-09-13

### Added

- add ADS mode cycle with zoom compensation and hit-marker placement

### Changed

- THIRD-PARTY-NOTICES now records MinHook as v1.3.4, verified by hashing the
  committed tree against upstream, in place of an unfilled placeholder.
- THIRD-PARTY-NOTICES now states Red Eclipse's own licensing accurately: free
  and open source, zlib for the engine and game source, CC-BY-SA 4.0 or later
  for the content, with the correct copyright holders and the Cube Engine 2 and
  Tesseract lineage. It previously described the game as requiring a purchase
  and credited work to reverse engineering that was never done, since the mod
  resolves the engine's public symbol names from the PDB the game itself ships.
- Both the README and the notices now acknowledge the Red Eclipse Mark Policy
  and state that this mod is unofficial and unaffiliated.

### Fixed

- mirror the vertical limit and restore the MIT grant
- gate per-frame sampling on DataFreshnessMs, drop dead AimDecoupling field
- Restored the MinHook licence file at `extern/minhook/LICENSE.txt` to the
  verbatim upstream text. Both BSD-2-Clause blocks covering Vyacheslav Patkov's
  Hacker Disassembler Engine had been cut down to a summary line, dropping the
  conditions and the disclaimer that the licence requires a source
  redistribution to retain.
- The Nexus ZIP now carries `LICENSE`, `THIRD-PARTY-NOTICES.md` and the
  licences of the components compiled into the `.asi`. It previously shipped
  the binary alone, which met neither MinHook's BSD-2-Clause terms nor the MIT
  terms of cameraunlock-core.
- The packager now fails instead of silently skipping a licence or notice file
  it cannot find, in either ZIP and beside the vendored loader.

## [0.3.0] - 2026-08-20

### Added

- drop the recenter hotkey and split smoothing into local and remote

### Changed

- Removed the in-game recentre control. Your tracker app owns the centre now:
  centre it there (opentrack's Center bind, the CENTER button in Headcam,
  SteamVR's reset) and the mod applies the pose it receives as absolute. A
  second centre inside the mod could only drift out of step with the tracker's.
  The `Home` key, the `Ctrl+Shift+T` chord and the `[Hotkeys] Recenter` /
  `[Hotkeys] ChordRecenter` INI entries are gone.

- A successful UDP bind is now logged with the port. Only the failure was
  logged, so a reader had to infer the healthy case from an absent warning.
- The log now keeps one previous generation as `RedEclipseHeadTracking.prev.log`.
  It is still truncated per launch, so relaunching after a crash no longer
  erases the crash report the handler wrote into it.
- Smoothing is now two keys: `[Smoothing] LocalSmoothing` (default 0.0) and
  `[Smoothing] RemoteSmoothing` (default 0.15), selected per connection from the
  tracker's source address. Both cover rotation and position; the old
  `[Smoothing] Smoothing` and `[Position] Smoothing` keys are removed. The hidden
  0.15 baseline floor is gone, so a tracker on this machine now gets
  zero-latency tracking by default.

## [0.2.0] - 2026-08-09

### Other

- Hello world

All notable changes to this project are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-08-09

### Added

- Head tracking for Red Eclipse over the OpenTrack UDP protocol, with decoupled
  look and aim: the head moves the view while the mouse still controls where
  shots land.
- Crosshair compensation. The crosshair is drawn where the clean aim point
  projects into the head-tracked view, so it stays on the spot the shot will
  hit through roll, horizon-locked yaw and 6DOF lean.
- 6DOF positional tracking with per-axis sensitivity, limits and smoothing.
- Hotkeys for recenter, toggle, tracking mode and yaw mode, on both the nav
  cluster and Ctrl+Shift chords.
- Runtime symbol resolution from the PDB Red Eclipse ships beside its
  executable, so the mod keeps working across game patches without a pinned
  offset registry.
