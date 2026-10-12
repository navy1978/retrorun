# Running RetroRun

[Back to README](../README.md)

The general command syntax is:

```text
retrorun [options] CORE_LIBRETRO GAME
```

Common options:

| Option | Description |
| --- | --- |
| `-s DIR`, `--savedir DIR` | Save and save-state directory. |
| `-d DIR`, `--systemdir DIR` | BIOS/system directory exposed to the core. |
| `-c FILE` | Configuration file path. |
| `-a RATIO`, `--aspect RATIO` | Force a numeric aspect ratio. |
| `-v VALUE`, `--volume VALUE` | Initial volume. |
| `-b VALUE`, `--backlight VALUE` | Initial backlight level on supported devices. |
| `-f`, `--fps` | Show the FPS counter. |
| `-t`, `--triggers` | Enable trigger support. |
| `-n`, `--analog` | Disable forced left-analog-to-D-pad mapping. |
| `-A MODE`, `--analog-to-digital MODE` | Analog-to-D-pad mode: `none`, `left`, `right`, `left_forced` or `right_forced`. Overrides the configuration file. |
| `-g` | Enable the GPIO joypad path used by some native devices. |
| `-r`, `--restart` | Enable the restart behaviour used by distribution launchers. |

Validated and retained per-game configurations for Flycast 2022 Low-End are
stored in
[`profiles/flycast2022-lowend`](../profiles/flycast2022-lowend/README.md).
The profile filenames include the Dreamcast product number so launchers can
select them independently of the ROM filename.

The modified Flycast core can also expose the Product number to RetroRun
before content startup. Enable automatic selection with:

```ini
retrorun_flycast_game_profile = best_validated
```

`disabled` keeps the normal configuration, `best_validated` selects the
visually approved profile and `best_performance` selects the fastest retained
profile, including documented compromises. RetroRun contains catalog version
`20260930` and checks for a strictly newer `flycast-game-catalog.ini` beside
its executable. With catalog updates set to `auto` (the default), an
at-most-daily background check downloads a newer valid catalog from the
`navy1978/retrorun` repository into the active configuration directory; it is
used on the next launch. Invalid files, unknown games and metadata-only
baseline entries leave the normal configuration untouched. Only RetroRun-aware Flycast builds that expose the
pre-launch Product-number extension show `Info > Flycast catalog`; stock
Flycast and every other core hide it. The submenu shows the short recognition,
Product number, profile, version and source status, while `Catalog` lists only
games with a validated global, chip or device-specific profile and their known
retail Product numbers. Untested title-only metadata is deliberately hidden.

Catalog schema 3 shares tuned profiles by SoC: RK3326 uses the profiles
validated on RG351MP, RK3566 those validated on RG353M, and RK3399 those
validated on RG552. A sparse device-specific profile may override individual
settings after the chip profile. Existing device-name detection remains the
first choice; only an unmapped name falls back to the NUL-separated Device
Tree `compatible` property in `/proc/device-tree/compatible` (or its sysfs
mirror). The effective order is global, chip, then device override.

A profile may also request a validated Flycast binary variant before content
startup. In addition to the existing upstream snapshots, `renderq_wait8`
selects `flycast_renderq_wait8_libretro.so` beside the requested core, or the
path configured as `retrorun_flycast_renderq_wait8_core`. RetroRun performs a
single guarded restart and then reapplies the complete transient game profile.

## macOS example

```sh
mkdir -p saves system

./retrorun \
  -s ./saves \
  -d ./system \
  "$HOME/Library/Application Support/RetroArch/cores/bsnes2014_performance_libretro.dylib" \
  "$HOME/Downloads/Super Ghouls 'N Ghosts (USA).sfc"
```

## Linux handheld example

Load the distribution environment first, then launch the SDL2 executable:

```sh
. /etc/profile

SDL_VIDEODRIVER=kmsdrm SDL_AUDIODRIVER=alsa \
./retrorun-sdl2 \
  -s /storage/roms/saves \
  -d /roms/bios \
  /path/to/core_libretro.so \
  /path/to/game.rom
```

If the distribution already exports the correct SDL drivers, omit the
`SDL_VIDEODRIVER` and `SDL_AUDIODRIVER` assignments. AmberELEC may use ALSA or
PulseAudio depending on its configuration.

The Linux SDL2 target starts fullscreen using the current display mode. To test
it in a window under X11 or Wayland:

```sh
RETRORUN_WINDOWED=1 \
RETRORUN_WINDOW_WIDTH=960 \
RETRORUN_WINDOW_HEIGHT=720 \
./retrorun-sdl2 -s ./saves -d ./system /path/to/core.so /path/to/game.rom
```

## Launcher script example

```sh
#!/bin/sh

. /etc/profile

CORE="$1"
ROM="$2"
PLATFORM="$3"

exec /usr/bin/retrorun-sdl2 \
  --triggers \
  -s "/storage/roms/$PLATFORM" \
  -d /roms/bios \
  "$CORE" \
  "$ROM"
```

## Tested cores

Core compatibility depends on the backend, architecture and distribution. The
following families have historically been tested with RetroRun:

- Dreamcast, Naomi and Atomiswave: Flycast and Flycast 2021.
- Nintendo 64: ParaLLEl N64.
- PlayStation: SwanStation, DuckStation and PCSX-ReARMed.
- PSP: PPSSPP.
- Super Nintendo: Snes9x variants and bsnes2014.
- Game Boy Advance: mGBA and VBA-M.
- Mega Drive/Genesis: Genesis Plus GX and PicoDrive.
- Atari: Stella and Virtual Jaguar.
- DOS: DOSBox Pure and DOSBox Core.
- Other cores including Beetle VB and Yaba Sanshiro.

A core shared library must use the same CPU architecture as RetroRun. For
example, a 32-bit core requires a 32-bit RetroRun executable and compatible
libraries.
