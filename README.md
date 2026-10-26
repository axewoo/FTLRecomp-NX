# FTLRecomp-NX

**FTL: Faster Than Light, running natively on the Nintendo Switch as homebrew.**

This is a Switch port of [FTL-Recompiled](https://github.com/EchoEclipseWolf/FTL-Recompiled), an open C++ reimplementation of FTL 1.6.9 by EchoEclipseWolf. It runs from hbmenu on Atmosphère, at the game's native 1280x720, with full controller and touch screen support.

**No game files are included.** The game reads its art, sound and text from `ftl.dat`, which you copy from your own copy of FTL (Steam, GOG or Humble).

![In-game](pics/Ingame1.png)

## Features

- Runs as a normal homebrew app (`ftl.nro`) on any Switch running Atmosphère
- Joy-Con / Pro Controller: snap selection that jumps between buttons, rooms and boxes, or a free stick-driven cursor (tap **−** to switch), click and drag, hotkeys on buttons
- Touch screen in handheld mode: tap to click, drag to move crew and equipment
- In-game controls card (hold **−**)
- Smooth play: common artwork is decoded in the background after launch, so effects and enemy ships don't stutter the first time they appear
- Optional FPS / frame time display (**ZL + ZR + right stick click**)
- Saves and profile stored next to the app on the SD card
- The same controller support also works in the PC build

## Install (prebuilt)

1. Download `ftl.nro` from the [latest release](../../releases/latest).
2. Find `ftl.dat` in your FTL installation:
   - Steam on Linux: `~/.steam/steam/steamapps/common/FTL Faster Than Light/data/ftl.dat`
   - Steam on Windows: `C:\Program Files (x86)\Steam\steamapps\common\FTL Faster Than Light\ftl.dat`
   - GOG/Humble: in the game folder (`ftl.dat`, or `data/ftl.dat`)
3. Copy both into the same folder on your SD card:

   ```
   sd:/switch/ftl/ftl.nro
   sd:/switch/ftl/ftl.dat
   ```

   On Linux, `scripts/install-sd.sh /path/to/sd ~/Downloads/ftl.nro` finds `ftl.dat` in your Steam libraries and copies both for you.
4. Launch **FTL** from hbmenu. Open hbmenu through a game while holding **R** (title override) so the game gets the full memory, not through the Album.

To put it on the home screen, make a forwarder with a tool such as [sphaira](https://github.com/ITotalJustice/sphaira).

## Controls

The game starts in snap selection: the left stick jumps a highlight between the buttons, rooms, crew and boxes on screen, and the right stick still moves a free cursor for beam aiming or anything snap can't reach. Tap **−** to switch to the free cursor and back.

| Button | Action |
| --- | --- |
| Left stick | Snap: jump to the next button or box. Free cursor: move cursor |
| Right stick | Snap: move a free cursor. Free cursor: move cursor slowly (precise aiming) |
| ZR (hold) | Faster cursor |
| A | Click, hold to drag |
| B | Right click: deselect, cancel, power down |
| ZL + A | Shift-click |
| X | Pause |
| Y | Jump / star map |
| L | Select all crew |
| R | Ship upgrades |
| D-pad ↑ → ↓ ← | Weapons 1, 2, 3, 4 |
| Left stick click | Toggle autofire |
| Right stick click | Cloak |
| ZL + ZR + right stick click | Show or hide the FPS / frame time display (F11 on PC) |
| + | Menu |
| − (tap) | Switch between snap selection and free cursor |
| − (hold) | Show the controls card |
| Touch screen | Tap to click, drag to move |

On PC with an Xbox or PlayStation pad, A means the button labelled A or Cross, and B means B or Circle.

## Performance

- **FPS display.** Press **ZL + ZR + right stick click** (F11 on PC) to show or hide it. It shows the frame rate, the average and worst frame time, how long the game logic and the drawing take each frame, the number of frames over 25 ms in the last 10 seconds, and texture and RAM use.
- **Background loading.** The original game decodes each picture the first time it's drawn, which stalls a frame on the Switch. Here a second CPU core decodes the common artwork (effects, weapons, crew, UI, enemy ships) in the first seconds after launch. Launched from the Album, enemy ships are left out to save memory and load when first needed.
- **Memory.** Each picture is kept once, on the GPU, instead of twice.

## Changes

### v1.1.0

- Snap selection controls: the left stick jumps between buttons, rooms, crew and boxes; tap − for the free cursor.
- Enemy crew (Rockmen, Zoltans and others) no longer freeze in place instead of repairing systems and fighting fires.
- Background loading of artwork, half the memory per picture, and the FPS display (see [Performance](#performance)).
- Restart from the pause menu after Continue no longer closes the game.

### v1.0.1

- Teleporter pads are drawn on the ship.
- Boarders (especially Rockmen) no longer get stuck re-planning at doorways instead of fighting.
- Boarders play their shooting animation while they sabotage a system.

### v1.0.0

- First release: native Switch port with controller and touch screen support.

## Build from source

You need Linux (or WSL), [devkitPro](https://devkitpro.org/wiki/Getting_Started) with its pacman, CMake 3.25+, Ninja and Git.

```sh
# Arch / CachyOS (devkitPro repos added to pacman); on other distros use dkp-pacman
sudo pacman -S --needed switch-dev switch-cmake switch-pkg-config switch-mesa switch-libdrm_nouveau cmake ninja git

git clone https://github.com/axewoo/FTLRecomp-NX
cd FTLRecomp-NX
bash scripts/build-switch.sh       # -> build/ftl.nro
bash scripts/install-sd.sh         # copies ftl.nro + your Steam ftl.dat to a mounted SD card
```

The first build downloads the libraries (`scripts/fetch-deps.sh`) and builds SDL3 for Switch ([nx-mod/sdl3-nx](https://github.com/nx-mod/sdl3-nx)), which takes a few minutes. If devkitPro is not in `/opt/devkitpro`, set `DEVKITPRO` first.

Every push to `main` is also built by GitHub Actions; the `ftl.nro` is attached to the run, and to the release for `v*` tags.

### PC build

Handy for testing with the same code and controls:

```sh
bash scripts/build-linux.sh
cp "/path/to/ftl.dat" build/Release/
./build/Release/FTLRecomp
```

## Files on the SD card

| File | What it is |
| --- | --- |
| `switch/ftl/ftl.nro` | The app |
| `switch/ftl/ftl.dat` | Your game data (required) |
| `switch/ftl/recomp-*.json` | Settings, profile and saved run |
| `switch/ftl/ftl.log` | Log, useful for bug reports |

## Troubleshooting

| Problem | Fix |
| --- | --- |
| Stutters in the first seconds after launch | Normal: the artwork is still loading in the background. It's smooth once that finishes. |
| Crash or black screen at start | Check that `ftl.dat` is in `switch/ftl/`, and launch through title override (hold R on a game). |
| `missing_asset` in `ftl.log` | Your `ftl.dat` is from another FTL version. Use the current Steam version (1.6.x). |
| Holding − shows "API Notifications Enabled/Disabled" | That's Ultrahand Overlay's hotkey. Turn it off: Ultrahand main screen, press +, then System > Notifications. |
| Crash report | Attach `switch/ftl/ftl.log` and the newest file in `atmosphere/crash_reports/` to an issue. |

Known limits: no text entry on Switch (ship renaming keeps the default name). Docked mode works at 720p.

## Credits

- **[Subset Games](https://subsetgames.com/)** made FTL. Please buy it.
- **[Icon](https://github.com/axewoo/FTLRecomp-NX/blob/main/switch/icon.jpg/)**, FTL iPad app icon © Subset Games
- **[EchoEclipseWolf](https://github.com/EchoEclipseWolf/FTL-Recompiled)** wrote FTL-Recompiled, the reimplementation this port is built on.
- **[nx-mod/sdl3-nx](https://github.com/nx-mod/sdl3-nx)**, devkitPro and switchbrew (libnx) for SDL3 and the Switch toolchain.
- Libraries: SDL3, FreeType, pugixml, nlohmann/json, stb.

## License

MIT, like FTL-Recompiled (see [LICENSE](LICENSE)). This covers the code only. FTL and its assets belong to Subset Games; do not share `ftl.dat`.
