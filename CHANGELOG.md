# Changelog

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
