# Red Eclipse Head Tracking

![Red Eclipse running with this mod](https://raw.githubusercontent.com/itsloopyo/red-eclipse-headtracking/main/assets/readme-clip.gif)

An unofficial head tracking mod for Red Eclipse that moves the view with your head while your mouse or controller keeps aiming, driven by OpenTrack over UDP, with no VR headset required.

## Features

- **Decoupled look and aim.** Turning your head swings the view without moving
  your aim. The game's shooting, hit detection and raycasts all run off the
  clean, mouse-controlled camera - the head transform only reaches the renderer.
- **6DOF.** Lean and move your head to shift the viewpoint, with travel limits
  and smoothing.
- **Works with any OpenTrack compatible tracker** - free options available for PC, iOS and Android

## A note on multiplayer

Red Eclipse is mostly played online. Head tracking gives you a wider view
without moving your aim, which is a real advantage over a player using a plain
mouse - much like a TrackIR user in a flight sim. It is not an aimbot and it
does not touch the network protocol, hitboxes or any game logic, but if you play
on servers where this would not be welcome, turn it off with `End` before you
connect.

## Requirements

- Red Eclipse, 64-bit Windows build. The game is free and open source: get it
  from <https://www.redeclipse.net> or free on Steam,
  <https://store.steampowered.com/app/967460/>
- Windows 10 or 11, 64-bit
- A head tracker that speaks the OpenTrack UDP protocol: OpenTrack itself with a
  webcam, or a phone app that sends the same packets

## Installation

### Lopari

Download [Lopari](https://lopari.app), choose **Red Eclipse**, and click
**Play with head tracking**.

### Standalone Installer

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

The mod creates its settings file, `CameraUnlock.ini`, and writes
`RedEclipseHeadTracking.log` next to itself on first run.

## Setting Up OpenTrack

The mod listens for OpenTrack pose data on UDP port `4242`, on every network
interface. One datagram is six little-endian 64-bit floats in the order
`x, y, z, yaw, pitch, roll`: position in centimetres, rotation in degrees, 48
bytes in total. Anything that sends that to that port drives the view.
OpenTrack's **UDP over network** output sends exactly this, and the steps below
set it up.

1. Install [OpenTrack](https://github.com/opentrack/opentrack/releases).
2. Pick a tracker under **Input**, using the notes below.
3. Set **Output** to **UDP over network**, host `127.0.0.1`, port `4242`.
4. Press **Start**. Tracking and the game can start in either order.

### Webcam

OpenTrack ships a `neuralnet tracker` input that reads a plain webcam. Select it
under **Input**, pick your camera in its settings, and use the output settings
above. How well it tracks depends on your camera and your lighting, so try it
before buying anything.

### Phone

A phone app can reach the mod directly, with no OpenTrack on the PC, if it sends
the datagram described above. Point it at this PC's IP address (run `ipconfig`
to find it) on port `4242`. Not every phone tracker speaks this protocol, so
check yours for an OpenTrack or UDP output option first. [Headcam](https://headcam.app)
sends it, and I wrote it so decent tracking is free for anyone who already owns
a phone.

Sending direct works when the app filters its own signal on the device. The
mod's smoothing is sized to take the edge off a clean signal rather than to
rescue a noisy one, so a raw feed sent direct will jitter. If it does, point the
app at OpenTrack's **UDP over network** *input* on some other port, say 5252,
and let OpenTrack's filters and curves clean it up before its output forwards to
`127.0.0.1:4242`.

Anything arriving from outside `127.0.0.0/8` counts as a remote connection and
is smoothed with `RemoteSmoothing` rather than `LocalSmoothing`. That includes a
tracker on this very PC that sends to the machine's own LAN address, because the
mod reads the source address and not the machine.

### Headset or other hardware

If your device has an OpenTrack input driver, select it under **Input** and use
the same output settings. OpenTrack's own **Input** list is the authority on
what it can read; the mod only ever sees what OpenTrack sends.

### Centring

Centring belongs to your tracker. The mod subtracts no centre of its own: it
applies the pose it receives exactly as it arrives, so a stream of zeros holds
the view where the game itself puts it. Press the centre control in your tracker
(OpenTrack's **Center** bind, or the CENTER button in Headcam) and the tracker
zeroes its own output, which leaves the view centred with the mod doing nothing.

That is why there is no centre hotkey here and nothing to re-centre in game. Two
centres in series would drift apart, because each side re-centres at moments the
other cannot see, and you would end up pressing twice to centre once. If the
view sits off to one side, centre it in the tracker.

## Controls

| Action | Key | Chord |
|--------|-----|-------|
| Toggle tracking | `End` | `Ctrl+Shift+Y` |
| Cycle tracking mode (6DOF / rotation only / position only) | `Page Up` | `Ctrl+Shift+G` |
| Toggle yaw mode (horizon-locked / camera-local) | `Page Down` | `Ctrl+Shift+H` |

The chords are there for keyboards without a nav cluster. Each action's keys
are a list in `CameraUnlock.ini`, so any of them, the chords included, can be
changed or removed there.

The tracking mode and the yaw mode you pick are saved to `CameraUnlock.ini` as
soon as you pick them, and come back the next time the game starts. `End` changes the current session only: at startup head
tracking is on or off as `EnableOnStartup` says.

There is no recentre key. Your tracker app owns the centre: use its own control
(opentrack's Center bind, the CENTER button in Headcam, SteamVR's reset) and the
mod applies whatever pose it receives.

### Aiming down sights

Head tracking stays on while you zoom. The zoom crosshair stays on your aim, so
with your head turned it sits off to one side, on the spot your shots will hit.
Head movement is scaled to the zoom, so the scope does not magnify it. Leaning
eases out while zoomed, because it would move your eye off the aim.

## Configuration

Apart from creating `CameraUnlock.ini` at startup when there is none, the mod
writes to it only when a hotkey changes the tracking mode or the yaw mode. It
never writes `RedEclipseHeadTracking.ini` or `Defaults.ini`.

<!-- cameraunlock:config -->
The mod reads its settings from `bin\amd64\CameraUnlock.ini` in the game folder, and creates the file when it starts and finds none. Edit it with any text editor.

A setting set to `default` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it, and neither do earlier versions of this mod. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.

`Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.

When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that. Edit it with any text editor.

Earlier versions of the mod kept these settings in `RedEclipseHeadTracking.ini`, in the same folder. The first time this version starts and finds no `CameraUnlock.ini`, it reads your settings from `RedEclipseHeadTracking.ini` and writes them into `CameraUnlock.ini`. It never changes `RedEclipseHeadTracking.ini`, and does not read it again while `CameraUnlock.ini` exists.

A setting that the defaults below set to `default` is written as `default` when the value imported for it equals its default at that start, which is the value `Defaults.ini` gives it, or the built-in value where `Defaults.ini` gives none. It then follows `Defaults.ini`. Every other setting is written with the value imported for it. `RotationEnabled` and `PositionEnabled` are one setting here, the tracking mode, so both are written as `default` or neither is.

Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:

- Reticle settings, and a key that toggled the reticle.
- A sensitivity, scale, deadzone, response curve or axis inversion you changed from its default. Set these in your tracker instead.
- The setting for a feature that earlier versions shipped switched off while it was untested. It now follows the mod's default.

An older version of the mod reads `RedEclipseHeadTracking.ini` and never reads `CameraUnlock.ini`, so a setting you change after updating is not in `RedEclipseHeadTracking.ini`.

Deleting only `CameraUnlock.ini` makes the next start read `RedEclipseHeadTracking.ini` again. To go back to the defaults, replace everything in `CameraUnlock.ini` with the defaults below. Every setting they set to `default` then follows `Defaults.ini`.

The built-in value of each setting set to `default` below:

- `UdpPort=4242`
- `EnableOnStartup=true`
- `WorldSpaceYaw=true`
- `RotationEnabled=true`
- `DataFreshnessMs=500`
- `LocalSmoothing=0.0`
- `RemoteSmoothing=0.15`
- `PositionEnabled=true`
- `PositionLimitX=0.3`
- `PositionLimitY=0.2`
- `PositionLimitYDown=0.2`
- `PositionLimitZ=0.4`
- `PositionLimitZBack=0.1`
- `ToggleKey=End, Ctrl+Shift+Y`
- `CycleTrackingModeKey=PageUp, Ctrl+Shift+G`
- `YawModeKey=PageDown, Ctrl+Shift+H`

With every setting at its default, the file reads:

```ini
; Red Eclipse head tracking settings.
; Comments start with ; and go on their own line. Text after a value is part of the value.
; Hotkeys are key names such as End, PageUp or Ctrl+Shift+Y. Separate several with commas; leave empty for none.
; A setting set to default takes its value from Defaults.ini, which every head tracking mod
; that keeps its settings in CameraUnlock.ini reads: %AppData%\CameraUnlock\Defaults.ini on
; Windows, $XDG_CONFIG_HOME/CameraUnlock/Defaults.ini (normally ~/.config/CameraUnlock) on
; Linux, under Wine and Proton too, and ~/Library/Application Support/CameraUnlock/Defaults.ini
; on macOS. The log names the file it read. Write a value instead of default to change that
; setting for this game only.

[CameraUnlock]
; Written by the mod. Leave this section in place.
ConfigFormat=1

[Network]
; UDP port the mod receives tracker data on (OpenTrack protocol).
UdpPort=default

[General]
; true: head tracking is on when the game starts. ToggleKey turns it on and off.
EnableOnStartup=default
; true: yaw turns around the world's up axis. false: around the camera's own up axis.
WorldSpaceYaw=default
; true: turning your head turns the view.
; Tracking mode at startup, with PositionEnabled. The mode hotkey changes both.
RotationEnabled=default
; Milliseconds a tracker packet stays current. Once the tracker has sent nothing
; for this long, the mod stops following it until data arrives again.
DataFreshnessMs=default

[Smoothing]
; Smoothing when the tracker runs on this PC. 0 is the least, 1 the most.
LocalSmoothing=default
; Smoothing when the tracker is another device on the network, such as a phone.
; 0 is the least, 1 the most.
RemoteSmoothing=default

[Position]
; true: moving your head moves the view.
; Tracking mode at startup, with RotationEnabled. The mode hotkey changes both.
PositionEnabled=default
; How far, in metres, leaning left or right can move the view.
PositionLimitX=default
; How far, in metres, raising your head can move the view.
PositionLimitY=default
; How far, in metres, lowering your head can move the view.
PositionLimitYDown=default
; How far, in metres, leaning forward can move the view.
PositionLimitZ=default
; How far, in metres, leaning back can move the view.
PositionLimitZBack=default

[Hotkeys]
; Turns head tracking on and off.
ToggleKey=default
; Changes the tracking mode: rotation and position, rotation only, position only.
CycleTrackingModeKey=default
; Switches yaw between the world's up axis and the camera's own (WorldSpaceYaw).
YawModeKey=default
```
<!-- /cameraunlock:config -->

Smoothing is picked automatically per connection from the tracker's source
address: `LocalSmoothing` for a tracker running on this machine, `RemoteSmoothing`
for a device on the network. Both cover rotation and position, and both accept
0.0 to 1.0.

There are no sensitivity, inversion, deadzone or scale settings: the mod applies
the pose your tracker sends, so set those in the tracker.

## Troubleshooting

**Nothing happens.** Read `bin\amd64\RedEclipseHeadTracking.log`. A working
session logs `Camera hooks installed`, then `OpenTrack: receiving data`, then
`Head tracking engaged` the first time the view actually moves. The log is
written fresh every launch; the launch before it is kept as
`RedEclipseHeadTracking.prev.log`, which is the one to send if the game
crashed and you relaunched.

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

**The crosshair is off to one side when I zoom.** Your head is turned: the
crosshair stays on your aim and you are looking past it. Turn back to it, or
move your aim to where you are looking.

**The view sits off to one side.** Centre it in your tracker app. The mod has no
centre of its own; it applies the pose the tracker sends.

## Updating and uninstalling

Re-run `install.cmd` to update. To remove:

```
uninstall.cmd
```

This removes the mod and, if this installer put it there, the ASI loader. Your
`CameraUnlock.ini`, and the `RedEclipseHeadTracking.ini` an earlier version
kept its settings in, are left alone so a reinstall keeps your settings.
`Defaults.ini` is not in the game folder, and the uninstall leaves it alone
too.

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

The host-side tests, including the crosshair projection checks and the test
that holds the settings conversion to what the published builds read, run with:

```
pixi run test
```

They need Node.js on the path for the settings file lint.

## Community & Support

- Discord: [Loop's Head Tracking Hangout](https://discord.com/invite/dxyZdyFNT9) - setup help, bug reports, and new-release announcements
- [Lopari](https://lopari.app) - free Windows launcher with one-click install and launch for the released head-tracking mods
- [Headcam](https://headcam.app) - free app that turns your iPhone or Android phone into the head tracker

## How it works

Red Eclipse ships its own source and debug symbols, so the mod resolves the
engine's camera functions by name instead of pinning addresses that a patch
would move. Four detours do the work:

- `game::recomputecamera` restores the untouched view matrix before the game
  derives the aim point from it, then samples the tracker for the frame.
- `setcammatrix` left-multiplies the head transform onto the view matrix the
  renderer is about to use, and rebuilds the camera axis vectors so particles
  and audio follow what is on screen.
- `hud::drawpointers` redraws the crosshair at the projection of the clean aim
  point through the head-tracked view-projection matrix.
- `UI::Render::draw` places the HUD hit confirmation at the same aim point,
  accounting for the game's visor distortion.

Because the crosshair is projected through the finished matrix rather than
re-derived from Euler angles, it stays correct under roll, horizon-locked yaw
and 6DOF lean without a second formula that has to agree with the first.

## License

This mod is MIT - see [LICENSE](LICENSE). Third-party components, and the
licensing of the game itself, are set out in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

It ships no game code, no game content and no part of the game's debug symbols.
The MIT licence covers this mod only and says nothing about Red Eclipse, which
is free and open source under its own terms: zlib for the engine and game
source, CC-BY-SA 4.0 or later for the content.

This is an unofficial, community-made mod. It is not affiliated with, endorsed
by or sponsored by the Red Eclipse project, and it uses the name only to say
which game it works with, as the [Red Eclipse Mark
Policy](https://www.redeclipse.net/docs/Trademark_Policy) permits for a product
designed to work with the game. If the project would prefer a different name or
presentation, we will change it on request.

## Credits

- **Quinton Reeves, Lee Salzman and Sławomir Błauciak**, and the wider Red
  Eclipse Team, for Red Eclipse, and for shipping the source and the debug
  symbols that made this mod straightforward to build. Red Eclipse builds on
  **Tesseract** and **Cube Engine 2** by Wouter van Oortmerssen, Lee Salzman
  and others.
- **ThirteenAG** for [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader).
- **TsudaKageyu** for [MinHook](https://github.com/TsudaKageyu/minhook).
- **The OpenTrack project** for the tracking protocol.
