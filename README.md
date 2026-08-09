# Red Eclipse Head Tracking

Look around independently of where you aim. Your head moves the view; the mouse
still controls the crosshair, and shots land exactly where the crosshair is
drawn.

## Features

- **Decoupled look and aim.** Turning your head swings the view without moving
  your aim. The game's shooting, hit detection and raycasts all run off the
  clean, mouse-controlled camera - the head transform only reaches the renderer.
- **6DOF.** Lean and move your head to shift the viewpoint, with per-axis
  sensitivity, travel limits and smoothing.

## A note on multiplayer

Red Eclipse is mostly played online. Head tracking gives you a wider view
without moving your aim, which is a real advantage over a player using a plain
mouse - much like a TrackIR user in a flight sim. It is not an aimbot and it
does not touch the network protocol, hitboxes or any game logic, but if you play
on servers where this would not be welcome, turn it off with `End` before you
connect.

## Requirements

- Red Eclipse (Steam, 64-bit build) - <https://store.steampowered.com/app/967460/>
- Windows 10 or 11, 64-bit
- A head tracker that speaks the OpenTrack UDP protocol: OpenTrack itself with a
  webcam, or a phone app that sends the same packets

## Installation

Run `install.cmd` from the release ZIP. It finds your Red Eclipse install,
drops the ASI loader and the mod into `bin\amd64\`, and records what it did so
uninstall can put things back.

```
install.cmd
```

Pass the game folder explicitly if auto-detection misses it:

```
install.cmd "D:\SteamLibrary\steamapps\common\Red Eclipse"
```

## Manual installation

From the release ZIP, into your Red Eclipse folder:

1. Copy `vendor\ultimate-asi-loader\dinput8.dll` to `bin\amd64\winmm.dll`
   (renamed - `redeclipse.exe` imports `winmm.dll`, so that is the slot the
   loader takes over).
2. Copy `plugins\RedEclipseHeadTracking.asi` to `bin\amd64\`.

The mod writes `RedEclipseHeadTracking.ini` and `RedEclipseHeadTracking.log`
next to itself on first run.

## OpenTrack setup

1. Install OpenTrack: <https://github.com/opentrack/opentrack/releases>
2. **Input:** whatever tracker you use (PointTracker, neuralnet, Aruco, ...).
3. **Output:** `UDP over network`.
4. Configure the output for `127.0.0.1` port `4242`.
5. Start tracking, then start Red Eclipse.

## Phone app setup

Any app that sends the OpenTrack UDP packet format works. Point it at your
PC's LAN address on port `4242` and make sure Windows Firewall allows inbound
UDP on that port for private networks.

## Controls

| Action | Key | Chord |
|--------|-----|-------|
| Recenter | `Home` | `Ctrl+Shift+T` |
| Toggle tracking | `End` | `Ctrl+Shift+Y` |
| Cycle tracking mode (6DOF / rotation only / position only) | `Page Up` | `Ctrl+Shift+G` |
| Toggle yaw mode (horizon-locked / camera-local) | `Page Down` | `Ctrl+Shift+H` |

The chords exist for keyboards without a nav cluster; both sets are always
active.

## Configuration

`bin\amd64\RedEclipseHeadTracking.ini`, written with defaults on first run.

| Setting | Default | Notes |
|---------|---------|-------|
| `General.EnableOnStartup` | `true` | |
| `General.Port` | `4242` | OpenTrack's standard port |
| `General.WorldSpaceYaw` | `true` | Horizon-locked yaw |
| `Sensitivity.Yaw` / `Pitch` / `Roll` | `1.0` | |
| `Sensitivity.InvertYaw` | `true` | Matches the tracker's sign to the engine's |
| `Sensitivity.InvertPitch` | `false` | |
| `Sensitivity.InvertRoll` | `true` | Matches the tracker's sign to the engine's |
| `Smoothing.Smoothing` | `0.0` | A 0.15 floor is applied internally |
| `Smoothing.DeadzoneDeg` | `0.0` | |
| `Position.Enabled` | `true` | |
| `Position.SensitivityX/Y/Z` | `1.0` | |
| `Position.LimitX` | `0.30` | Metres, symmetric |
| `Position.LimitY` | `0.20` | Metres, symmetric |
| `Position.LimitZ` | `0.40` | Metres forward |
| `Position.LimitZBack` | `0.10` | Metres back |
| `Position.Smoothing` | `0.15` | |
| `Position.PositionScale` | `8.0` | World units per metre - Cube's world is 8 to the metre |
| `Hotkeys.*` | see table above | Virtual-key codes in hex |

If an axis moves the wrong way, flip the matching `Invert*` value.

## Troubleshooting

**Nothing happens.** Read `bin\amd64\RedEclipseHeadTracking.log`. A working
session logs `Camera hooks installed`, then `OpenTrack: receiving data`, then
`Head tracking engaged` the first time the view actually moves.

**The log says it stayed dormant.** The mod could not read Red Eclipse's debug
symbols. It needs `redeclipse_windows_amd64.pdb` in the game's root folder,
which Steam installs by default - verify the game files in Steam to restore it.
Until then the mod installs no hooks at all and the game runs vanilla.

**No log file at all.** The ASI loader is not loading. Check that
`bin\amd64\winmm.dll` exists and is about 3.5 MB (the loader), not the tiny
Windows stub.

**`OpenTrack: no data`.** The tracker is not reaching the game. Check the port
matches, the tracker is actually running, and the firewall allows UDP 4242.

**Tracking does not move the view in menus.** That is deliberate - tracking is
suppressed whenever a menu, the console or the Steam overlay has input.

## Updating and uninstalling

Re-run `install.cmd` to update. To remove:

```
uninstall.cmd
```

This removes the mod and, if this installer put it there, the ASI loader. Your
`RedEclipseHeadTracking.ini` is left alone so a reinstall keeps your settings.

## Building from source

Needs Visual Studio 2022 or newer with the C++ workload, CMake 3.20+, and
[pixi](https://pixi.sh).

```
git clone --recurse-submodules https://github.com/itsloopyo/red-eclipse-headtracking
cd red-eclipse-headtracking
pixi run build-release
pixi run install          # deploy to your game folder for testing
pixi run package          # build the release ZIPs
```

The host-side maths tests, including the crosshair projection checks, run with:

```
cmake -B build-tests -A x64 -DHEADTRACKING_BUILD_TESTS=ON
cmake --build build-tests --config Release
ctest --test-dir build-tests -C Release --output-on-failure
```

## How it works

Red Eclipse ships its own source and debug symbols, so the mod resolves the
engine's camera functions by name instead of pinning addresses that a patch
would move. Three detours do the work:

- `game::recomputecamera` restores the untouched view matrix before the game
  derives the aim point from it, then samples the tracker for the frame.
- `setcammatrix` left-multiplies the head transform onto the view matrix the
  renderer is about to use, and rebuilds the camera axis vectors so particles
  and audio follow what is on screen.
- `hud::drawpointers` redraws the crosshair at the projection of the clean aim
  point through the head-tracked view-projection matrix.

Because the crosshair is projected through the finished matrix rather than
re-derived from Euler angles, it stays correct under roll, horizon-locked yaw
and 6DOF lean without a second formula that has to agree with the first.

## License

MIT - see [LICENSE](LICENSE). Third-party components are listed in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

## Credits

- **Red Eclipse Team** for Red Eclipse, and for shipping the source and symbols
  that made this mod straightforward to build.
- **ThirteenAG** for [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader).
- **TsudaKageyu** for [MinHook](https://github.com/TsudaKageyu/minhook).
- **The OpenTrack project** for the tracking protocol.
