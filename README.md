# RetroRun

RetroRun is a lightweight libretro frontend for Linux handhelds and desktop
systems. It was originally designed for Anbernic devices using the GO2/DRM
graphics stack and now provides a platform abstraction for input, audio and
video, with an additional SDL2 backend for macOS and Linux.

The native backend remains available for the RG351 M/P/V/MP, RG552, RG503,
RG353M and RG353V. The SDL2 backend provides an alternative implementation for
Linux handheld distributions such as AmberELEC, ArkOS/dArkOS and other devices
that provide SDL2, KMSDRM and OpenGL ES 3.

The Linux SDL2 backend must still be tested on each physical device. Controller
mapping, screen rotation, audio driver and performance can vary between models
and distributions.

- [Changelog](changelog.txt)
- [Porting guide](PORTING.md)

## Backends and build outputs

| Host or target | Backend | Build command | Output |
| --- | --- | --- | --- |
| Anbernic Linux, default | Native GO2/DRM | `make config=release` | `retrorun` |
| Linux handheld or desktop | SDL2 + OpenGL ES | `make sdl2 config=release` | `retrorun-sdl2` |
| macOS | SDL2 + Desktop OpenGL | `make` | `retrorun` |

On Linux, the GO2 backend intentionally remains the default for compatibility
with existing AmberELEC packages and supported Anbernic devices. Building the
SDL2 target produces a separate executable and does not replace `retrorun`.

## Features

- Software-rendered and OpenGL/OpenGL ES libretro cores.
- Integrated native GO2, DRM and Anbernic input support.
- Portable SDL2 input, audio and video backend.
- Runtime menu and on-screen display.
- Automatic and selectable aspect ratios, pixel-perfect scaling and Tate mode.
- Nearest/linear filtering, scanlines and CRT effects.
- Optional VSync and frontend frame pacing.
- Save states, automatic save/load and multiple state slots.
- FPS counter, pause, fast-forward and screenshots.
- Controller remapping, analog sticks, triggers and rumble.
- Device, core and game information screens.
- Per-core libretro options loaded from `retrorun.cfg`.

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

## Building

Clone the repository first:

```sh
git clone https://github.com/navy1978/retrorun.git
cd retrorun
```

### Native GO2/DRM backend

On a supported Linux toolchain, the default command builds the historical
Anbernic backend:

```sh
make config=release
```

The native build uses DRM/GBM, EGL/OpenGL ES, RGA, evdev, ALSA and OpenAL. It is
normally cross-compiled inside the AmberELEC or device-distribution build
environment, where those libraries and their target headers are available.

The backend can also be selected explicitly:

```sh
make go2 config=release
```

### Linux SDL2/KMSDRM backend

Required target dependencies are:

- a C++20 compiler;
- SDL2 development files;
- libpng and pkg-config;
- EGL, OpenGL ES 3 headers and `libGLESv2`;
- an SDL2 build with KMSDRM support when running without X11 or Wayland.

Build the alternative backend with either command:

```sh
make PLATFORM=sdl2 config=release
```

```sh
make sdl2 config=release
```

Both commands create `retrorun-sdl2`. The build variables `CXX`, `SDL_CONFIG`,
`PKG_CONFIG`, `SDL_CFLAGS`, `SDL_LIBS`, `PNG_CFLAGS`, `PNG_LIBS`,
`GLES_CFLAGS` and `GLES_LIBS` can be overridden for a cross-compilation
sysroot. For example:

```sh
make sdl2 config=release \
  CXX=aarch64-linux-gnu-g++ \
  SDL_CONFIG=/path/to/sysroot/usr/bin/sdl2-config \
  PKG_CONFIG=/path/to/target-pkg-config
```

Do not copy a macOS or x86-64 Linux executable to an ARM handheld. RetroRun,
the libretro core and all linked libraries must be built for the target CPU and
ABI.

#### AmberELEC packaging

AmberELEC already builds SDL2 with KMSDRM and OpenGL ES. To include both
RetroRun variants in an AmberELEC image, its RetroRun package must:

1. retain the existing default `make config=release` native build;
2. add SDL2 to the package dependencies;
3. run `make PLATFORM=sdl2 config=release` using the AmberELEC target sysroot;
4. install `retrorun-sdl2` alongside `/usr/bin/retrorun`.

Keeping the two executable names separate makes it possible to select the
backend per system or per game without changing the existing launcher.

#### ArkOS and dArkOS

ArkOS historically includes `retrorun` and `retrorun32`. The SDL2 variant can
be built using a matching 32-bit or 64-bit distribution toolchain. Check the
architecture of the selected core before choosing the compiler. Since device
images can ship different SDL2 builds, confirm that the target SDL2 provides
the `kmsdrm` video driver.

### macOS SDL2 backend

Install SDL2 and libpng with Homebrew:

```sh
brew install sdl2 libpng
make
```

This creates `retrorun` using SDL2 and Desktop OpenGL. The default window is
960x720 and can be overridden with:

```sh
RETRORUN_WINDOW_WIDTH=1280 RETRORUN_WINDOW_HEIGHT=960 ./retrorun ...
```

## Running RetroRun

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
| `-g` | Enable the GPIO joypad path used by some native devices. |
| `-r`, `--restart` | Enable the restart behaviour used by distribution launchers. |

### macOS example

```sh
mkdir -p saves system

./retrorun \
  -s ./saves \
  -d ./system \
  "$HOME/Library/Application Support/RetroArch/cores/bsnes2014_performance_libretro.dylib" \
  "$HOME/Downloads/Super Ghouls 'N Ghosts (USA).sfc"
```

### Linux handheld example

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

### Launcher script example

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

## Configuration

RetroRun reads a simple `key = value` configuration file.

- Native GO2 default: `/storage/.config/distribution/configs/retrorun.cfg`.
- SDL2 default: `./retrorun.cfg` in the current working directory.
- Every backend: pass `-c /path/to/retrorun.cfg` to select another file.

The repository includes [retrorun.cfg](retrorun.cfg) as an SDL2-oriented
example. A minimal portable configuration is:

```ini
retrorun_log_level = INFO
retrorun_show_loading_screen = true
retrorun_aspect_ratio = auto
retrorun_pixel_perfect = false
retrorun_fps_counter = false
retrorun_loop_declared_fps = true

# SDL2 only
retrorun_video_renderer = auto
retrorun_vsync = false

# All video backends
retrorun_video_filter = off
retrorun_video_shader = off
```

Frontend video settings:

| Setting | Values and behaviour |
| --- | --- |
| `retrorun_aspect_ratio` | `auto`, `2:1`, `4:3`, `5:4`, `16:9`, `16:10`, `1:1` or `3:2`. |
| `retrorun_pixel_perfect` | `true` or `false`; default is disabled. |
| `retrorun_tate_mode` | `auto`, `enabled`, `disabled` or `reverted`. |
| `retrorun_fps_counter` | Enables the on-screen FPS counter. |
| `retrorun_show_loading_screen` | Shows the loading screen while a core initializes. |
| `retrorun_video_filter` | `off`, `nearest` or `linear`; default is `off`. |
| `retrorun_video_shader` | `off`, `scanlines` or `crt`; default is `off`. |
| `retrorun_loop_declared_fps` | Paces the frontend using the frame rate declared by the core. |
| `retrorun_video_renderer` | SDL2 only: `auto`, `software`, `opengl` or `vulkan`. Requires restart. Vulkan is not implemented yet. On Linux `opengl` means OpenGL ES. |
| `retrorun_vsync` | SDL2 only; default is `false` and can also be changed from the menu. |

General and input settings:

| Setting | Description |
| --- | --- |
| `retrorun_log_level` | `INFO`, `DEBUG`, `WARNING` or `ERROR`. |
| `retrorun_device_name` | Overrides automatic device identification. |
| `retrorun_screenshot_folder` | Destination for screenshots. |
| `retrorun_audio_buffer` | Audio buffer value such as `-1`, `256`, `512` or `1024`. |
| `retrorun_auto_save` | Saves automatically during shutdown. |
| `retrorun_auto_load` | Loads the automatic save at startup. |
| `retrorun_force_left_analog_stick` | Maps the left analog stick to the D-pad. |
| `retrorun_swap_l1r1_with_l2r2` | Exchanges shoulder buttons and triggers. |
| `retrorun_swap_sticks` | Exchanges left and right analog sticks. |
| `retrorun_alternative_input_mode` | Uses the ArkOS-style Select/F2 hotkeys. |
| `retrorun_mouse_speed_factor` | Mouse emulation speed; default is `5`. |
| `retrorun_force_video_multithread` | Overrides the device-specific video-threading choice. |
| `retrorun_enable_key_log` | Logs logical button names at `DEBUG` level. |

Native-device overrides:

- `retrorun_extra_retrogame_name`
- `retrorun_extra_osh_name`
- `retrorun_extra_evdev_name`
- `retrorun_rumble_type` (`pwm` or `event`)
- `retrorun_rumble_event`
- `retrorun_rumble_pwm_file`
- `retrorun_disable_rumble`

Settings not beginning with `retrorun_` are forwarded as libretro core
options. Their names depend on the core library name. If a distribution calls
the core `reicast` rather than `flycast`, its option prefix may also need to be
changed.

Example Flycast options:

```ini
flycast_threaded_rendering = enabled
flycast_internal_resolution = 640x480
flycast_enable_dsp = disabled
flycast_synchronous_rendering = disabled
flycast_alpha_sorting = per-strip (fast, least accurate)
flycast_texupscale = off
flycast_enable_purupuru = enabled
flycast_gdrom_fast_loading = enabled
```

## Controls

SDL-compatible controllers are detected automatically. The default keyboard
mapping for the SDL2 backend is:

| Keyboard | RetroPad control |
| --- | --- |
| Arrow keys | D-pad |
| `X`, `Z`, `S`, `A` | A, B, X, Y |
| Return, Backspace | Start, Select |
| `Q`, `W` | L1, R1 |
| `1`, `2` | L2, R2 |
| `3`, `4` | L3, R3 |
| Escape or window close | Quit |

Default controller hotkeys:

| Combination | Action |
| --- | --- |
| L3 + R3 | Open or close the RetroRun menu. |
| Select + Start | Exit RetroRun; hold/repeat for confirmation. |
| Select + Y | Toggle FPS display. |
| Select + B | Take a screenshot. |
| Select + A | Pause or resume. |
| Select + R2 | Toggle fast-forward. |
| Select + R1 | Save the current state slot. |
| Select + L1 | Load the current state slot. |
| Select + Up/Down | Select the next/previous state slot. |

With `retrorun_alternative_input_mode = true`, Select or F2 is used with X to
open the menu, Y for FPS, B for screenshots, A for pause and Start for exit.

### Button remapping

Logical controls can be assigned in `retrorun.cfg`:

```ini
retrorun_mapping_button_up = DPadUp
retrorun_mapping_button_down = DPadDown
retrorun_mapping_button_a = A
retrorun_mapping_button_b = B
retrorun_mapping_button_x = SELECT
retrorun_mapping_button_start = START
retrorun_mapping_button_l1 = TopLeft
retrorun_mapping_button_r1 = TopRight
retrorun_mapping_button_l2 = TriggerLeft
retrorun_mapping_button_r2 = TriggerRight
retrorun_mapping_button_l3 = F1
retrorun_mapping_button_r3 = F2
```

To discover the logical name generated by a device, enable:

```ini
retrorun_enable_key_log = true
retrorun_log_level = DEBUG
```

The log then prints entries such as:

```text
Joypad button pressed: [BTN_TRIGGER_HAPPY2] - [F2]
```

Use the second value, `F2` in this example, in a mapping entry. Directional
values are named `DPadUp`, `DPadDown`, `DPadLeft` and `DPadRight` in mapping
entries.

## Porting and current limitations

The frontend accesses input, audio and graphics through [src/platform.h](src/platform.h).
The native implementation is in `src/platform_go2.cpp`; the macOS and Linux
implementation is in `src/platform_sdl.cpp`. Menus and OSD composition are
shared by both backends. See [PORTING.md](PORTING.md) for the backend contract.

Current SDL2 Linux limitations:

- actual compatibility must be validated on every handheld model;
- KMSDRM display rotation depends on the SDL2 patches shipped by the distro;
- battery and brightness integration is less complete than in the native GO2 backend;
- Vulkan is visible as a configuration value but is rejected because the
  libretro Vulkan interface is not implemented;
- the SDL2 build currently targets OpenGL ES 3 for hardware-rendered cores.

For supported Anbernic devices, GO2/DRM remains the safest production backend.
SDL2/KMSDRM is intended as an alternative and as the portable path for new
devices.

## History and credits

RetroRun was initially developed by OtherCrashOverride until 2020. Development
has been continued by navy1978 since 2021. This version integrates and extends
[libgo2](https://github.com/OtherCrashOverride/libgo2) and
[rg351p-js2box](https://github.com/lualiliu/RG351P_virtual-gamepad).

Thanks to Cebion, Christian_Haitian, dhwz, madcat1990, pkegg, superdealloc and
Szalik for their contributions and support.
