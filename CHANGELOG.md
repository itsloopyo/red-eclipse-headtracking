# Changelog

## [0.3.0] - 2026-08-20

### Added

- drop the recenter hotkey and split smoothing into local and remote

## [Unreleased]

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
