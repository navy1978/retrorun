# RetroRun

<p align="center">
  <img src="assets/retrorun.png" alt="RetroRun logo" width="240">
</p>

RetroRun is a lightweight libretro frontend for Linux handhelds and desktop
systems. It was originally designed for Anbernic devices using the GO2/DRM
graphics stack and now provides a platform abstraction for input, audio and
video, with an additional SDL2 backend for macOS and Linux.

## Highlights

- **Save states:** create and load states from the on-screen menu or controller
  shortcuts, choose between multiple slots, and optionally save and restore a
  state automatically when starting or closing a game.
- **Multi-disc games:** change the active CD, GD-ROM or other removable image
  while playing, using RetroRun's file browser and the libretro disk-control
  interface exposed by compatible cores.
- **Bezels and screen decorations:** automatically reuse installed AmberELEC
  and ArkOS bezel packs, select artwork per game or system, honour custom
  libretro and Batocera-style viewports, or download and manage a curated set
  directly from the menu. Alpha-composited artwork is cached and accelerated
  by RGA on supported GO2 devices.
- **RetroAchievements:** sign in from the frontend, identify supported games,
  browse locked and unlocked achievements with their badges, and display game
  identification and unlock notifications. Official achievements, optional
  unofficial achievements and Encore mode are supported in softcore mode, and
  the integration can be disabled.
- **Complete on-screen interface:** pause and resume games, inspect device,
  core, graphics and network information, change runtime video, audio, volume
  and brightness settings, manage save states and exit cleanly without leaving
  the frontend.
- **Fast-forward and frame control:** uncapped fast-forward with configurable
  frame presentation, optional adaptive or fixed frameskip, VSync and frontend
  frame pacing.
- **Flexible video output:** software and OpenGL/OpenGL ES cores, automatic or
  selectable aspect ratios, pixel-perfect scaling, Tate rotation, nearest or
  linear filtering, scanlines, CRT effects and optional screen decorations.
- **Handheld-oriented input:** controller remapping, analog-to-D-pad modes,
  Tate-aware directional controls, analog sticks, triggers, rumble and
  configurable controller shortcuts.
- **Useful frontend tools:** screenshots, an unrestricted FPS counter,
  a startup loading screen, icon-and-text notifications and an on-screen
  keyboard with upper- and lowercase input.
- **Portable backends:** native GO2/DRM support for compatible Anbernic devices
  and an SDL2 backend for Linux handhelds, Linux desktops and macOS.
- **Configurable cores and diagnostics:** per-core libretro options, separate
  frontend and core log levels, timestamped logs and selectable save, system
  and screenshot directories.

The native backend remains available for the RG351 M/P/V/MP, RG552, RG503,
RG353M and RG353V. The SDL2 backend provides an alternative implementation for
Linux handheld distributions such as AmberELEC, ArkOS/dArkOS and other devices
that provide SDL2, KMSDRM and OpenGL ES 3.

The Linux SDL2 backend must still be tested on each physical device. Controller
mapping, screen rotation, audio driver and performance can vary between models
and distributions.

- [Changelog](changelog.txt)
- [Porting guide](PORTING.md)

### Artwork attribution

On-screen interface icons are derived from
[Streamline Core Line Free](https://www.streamlinehq.com/icons/core-line-free),
licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
Icons by [Streamline](https://streamlinehq.com).

## Backends and build outputs

| Host or target | Backend | Build command | Output |
| --- | --- | --- | --- |
| Anbernic Linux, default | Native GO2/DRM | `make config=release` | `retrorun` |
| Linux handheld or desktop | SDL2 + OpenGL ES | `make sdl2 config=release` | `retrorun-sdl2` |
| macOS | SDL2 + Desktop OpenGL | `make` | `retrorun` |

On Linux, the GO2 backend intentionally remains the default for compatibility
with existing AmberELEC packages and supported Anbernic devices. Building the
SDL2 target produces a separate executable and does not replace `retrorun`.

## On-screen menu

RetroRun includes an OSD menu shared by the native GO2/DRM and SDL2 backends.
It can pause and resume the active core and provides access to device, core and
game information, runtime settings, save states, credits and clean shutdown.
Each menu remembers its selected item when entering a submenu and returning.
The read-only **Info > Graphics** page reports the active platform backend,
renderer, software or hardware core path, output and core resolutions, pixel
format, aspect ratio, filter, shader, VSync, pixel-perfect state and UI profile.
The default controller shortcut is L3 + R3; alternative Select/F2 shortcuts
can be enabled with `retrorun_alternative_input_mode`.

The menu uses clipped, time-based text scrolling for labels wider than the
available area. Its UI profile can be changed under **Settings > Video**:

- `auto` keeps the original compact layout on recognized Anbernic devices and
  selects the responsive desktop layout on macOS, Windows and regular Linux PCs;
- `handheld` always uses the original 240x160 menu canvas;
- `desktop` derives the menu canvas from the window size so that text remains
  approximately 16 pixels high instead of being enlarged with the game image.

<p align="center">
  <img src="assets/menu.png" alt="RetroRun on-screen main menu" width="720">
</p>

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
| `-A MODE`, `--analog-to-digital MODE` | Analog-to-D-pad mode: `none`, `left`, `right`, `left_forced` or `right_forced`. Overrides the configuration file. |
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

The configuration file (`retrorun.cfg`) contains settings for different cores.
Example:

```ini
# ---- RETRORUN INTERNAL SETTINGS ----
retrorun_screenshot_folder = /storage/roms/screenshots
# ---- FLYCAST ----
flycast_threaded_rendering = enabled
flycast_internal_resolution = 640x480
flycast_anisotropic_filtering = off
flycast_enable_dsp = disabled
flycast_synchronous_rendering = disabled
flycast_enable_rtt = disabled
flycast_enable_rttb = disabled
flycast_delay_frame_swapping = disabled
# Alpha sorting should be set to per strip
flycast_alpha_sorting = per-strip (fast, least accurate)
flycast_div_matching = auto
# Texupscale should be off
flycast_texupscale = off
# Vibration support should be on
flycast_enable_purupuru = enabled
flycast_auto_skip_frame = disabled
flycast_gdrom_fast_loading = enabled
flycast_volume_modifier_enable = disabled
flycast_framerate = fullspeed
flycast_anisotropic_filtering = disabled
# ---- FLYCAST2021 ----
flycast2021_threaded_rendering = enabled
flycast2021_internal_resolution = 640x480
flycast2021_anisotropic_filtering = off
flycast2021_enable_dsp = disabled
flycast2021_synchronous_rendering = disabled
flycast2021_enable_rtt = disabled
flycast2021_enable_rttb = disabled
flycast2021_delay_frame_swapping = disabled
# Alpha sorting should be set to per strip
flycast2021_alpha_sorting = per-strip (fast, least accurate)
flycast2021_div_matching = auto
# Texupscale should be off
flycast2021_texupscale = off
# Vibration support should be on
flycast2021_enable_purupuru = enabled
flycast2021_gdrom_fast_loading = enabled
flycast2021_volume_modifier_enable = disabled
flycast2021_framerate = fullspeed
flycast2021_anisotropic_filtering = disabled
# ---- PARALLEL N64 ----
parallel-n64-framerate = fullspeed
parallel-n64-filtering = nearest
parallel-n64-audio-buffer-size = 1024
parallel-n64-gfxplugin-accuracy = medium
parallel-n64-screensize = 640x480
parallel-n64-gfxplugin = rice
parallel-n64-pak1 = rumble
parallel-n64-pak2 = memory
parallel-n64-pak3 = none
parallel-n64-pak4 = none
# ---- JAGUAR ----
virtualjaguar_doom_res_hack = enabled
virtualjaguar_pal = disabled
virtualjaguar_usefastblitter = enabled
virtualjaguar_bios = enabled
# ---- PPSSPP ----
ppsspp_cpu_core = JIT
#ppsspp_detect_vsync_swap_interval = disabled
ppsspp_fast_memory = enabled
ppsspp_frameskip = 0
ppsspp_frameskiptype = Number of frames
ppsspp_ignore_bad_memory_access = enabled
ppsspp_internal_resolution = 480x272
ppsspp_rendering_mode=buffered
# ---- DUCKSTATION ----
duckstation_CPU.Overclock = 100
duckstation_Controller1.Type=AnalogController
# ---- SWANSTATION ----
swanstation_CPU_Overclock = 100
swanstation_GPU_Renderer = Software
```

(*) Pay attention to the parameter names, as they follow the naming convention
of the core. For example, in some distributions, the Flycast core is named
Reicast. In such cases, parameters should be prefixed accordingly—e.g.,
`flycast_threaded_rendering` should be renamed to
`reicast_threaded_rendering`.

### RetroAchievements

RetroRun integrates the official `rcheevos` client in softcore mode. Enable it
in `retrorun.cfg` and provide a RetroAchievements username and token:

```ini
retrorun_achievements_enabled = true
retrorun_achievements_username = your_username
# Token generated by a previous rcheevos login. This is not the Web API Key.
retrorun_achievements_token = your_login_token
retrorun_achievements_unofficial = false
retrorun_achievements_encore = false
```

The Web API Key displayed in the RetroAchievements website authentication page
is not a login token and cannot be used here. For the first login, omit the
token and set `retrorun_achievements_password`. After a successful login,
RetroRun stores the token returned by RetroAchievements and removes the
plaintext password from the configuration file. Login, game identification
and unlock requests run in the background. RetroRun displays a small
notification when a game is identified or an achievement is unlocked.
Hardcore mode is intentionally not enabled yet because it requires enforcing
restrictions on save states, fast-forward and other frontend features.

The service can also be configured at runtime under
**Settings > RetroAchievements**. The status row reports whether it is
disabled, waiting for credentials, signing in, or logged in. Username and
password are entered with the physical keyboard on SDL2 or the shared virtual
keyboard on GO2. Password text is masked; after a successful login it is
replaced in the configuration file by the rcheevos login token.

Set `retrorun_achievements_encore = true` to reactivate achievements already
unlocked by the current user. This is useful for testing notifications; the
server does not award the same achievement or its points twice.

RetroAchievements configuration reference:

| Setting | Description |
| --- | --- |
| `retrorun_achievements_enabled` | Enables the service; default is `false`. |
| `retrorun_achievements_username` | RetroAchievements account name. |
| `retrorun_achievements_password` | Plaintext password used only for the first login; removed after a token is obtained. |
| `retrorun_achievements_token` | Login token saved after authentication. This is not the Web API Key. |
| `retrorun_achievements_unofficial` | Includes unofficial achievements when `true`; default is `false`. |
| `retrorun_achievements_encore` | Reactivates already unlocked achievements for local testing; default is `false`. |

### Screen decorations

Static PNG decorations can fill the unused area around the game without
stretching the emulated image. Enable them under **Settings > Video > Screen
decorations** or with:

```ini
retrorun_decorations = auto
# Optional. The default is a decorations directory next to retrorun.cfg.
retrorun_decorations_path = /storage/.config/retrorun/decorations
```

RetroRun loads the first matching file and keeps the converted RGB565 surface
in memory. Explicit RetroRun files take priority; after them it can reuse a
bezel already installed by the host distribution:

```text
decorations/games/<system>/<rom name>.png
decorations/games/<rom name>.png
decorations/systems/<system>.png
decorations/default.png
```

AmberELEC packs are detected in `/tmp/overlays/bezels` and
`/storage/roms/bezels`. RetroRun reads the active pack and optional system
override from `distribution.conf`, supports its full-name, short-name,
numbered and default per-game `.cfg` files, then falls back to the system PNG.
Compatible packs in `/roms/bezels` or `/roms2/bezels` are also detected for
ArkOS and user installations, either below a named pack such as `default` or
with `systems` directly below `bezels`. If AmberELEC has generated
`/tmp/raappend.cfg`, its custom viewport is reused. Detection and parsing occur
once when the game starts; no distribution-specific work is done per frame.
RetroRun only reads artwork already installed by the distribution and does not
redistribute it.

An optional `.info` file beside a PNG can define the exact game viewport using
Batocera-style `width`, `height`, `top`, `left`, `bottom` and `right` values.
For example, `default.png` can be accompanied by `default.info`:

```json
{
  "width": 1920,
  "height": 1080,
  "top": 0,
  "left": 240,
  "bottom": 0,
  "right": 240
}
```

The values describe the artwork's reference resolution and border sizes;
RetroRun scales the resulting viewport to the current display and fits the
game inside it without changing the game's aspect ratio. The viewport never
affects menus or other frontend pages. Parsing happens only when the decoration
is loaded, not during normal frame rendering.

The **Settings > Video > Decorations** menu can download a small selection of
static borders directly from `libretro/common-overlays`. RetroRun downloads
only the selected PNG, converts its libretro viewport to a local `.info` file
and stores attribution beside it under `downloads/libretro`. It never downloads
or extracts the complete repository. Installed artwork is licensed under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) and can be updated or
removed from the same menu. Downloads run in the background while the menu
remains responsive.

The same menu separates the configured source from the artwork currently in
use. `Source: Automatic` follows an installed distribution pack, while
`Source: RetroRun`, `Source: AmberELEC: <pack>` and `Source: ArkOS: <pack>`
select one explicitly. `Using:` reports the source that actually matched the
current game. Distribution choices appear only when their directories are
present; the scan runs during menu initialization and never per frame.

`<system>` is the first directory after `roms` or `roms2` (falling back to the
ROM's parent directory), and `<rom name>` is the filename without its
extension. This also works when games are grouped in subdirectories. The local
layout is compatible with the core of Batocera's decoration naming scheme.
Images are installed separately and no third-party artwork is bundled with
RetroRun. If no PNG matches, no decoration is composited and the game keeps
its regular video layout. PNG alpha is preserved and matching artwork is
composited over the game, allowing curved frames to mask the rectangular core
image correctly. On GO2 this source-over operation is handled by RGA hardware;
the full artwork is cached in all three presenter framebuffers and normally
only the game-damaged rectangle is blended again.

Frontend video settings:

| Setting | Values and behaviour |
| --- | --- |
| `retrorun_aspect_ratio` | `auto`, `2:1`, `4:3`, `5:4`, `16:9`, `16:10`, `1:1` or `3:2`. |
| `retrorun_pixel_perfect` | `true` or `false`; default is disabled. |
| `retrorun_tate_mode` | `auto`, `enabled`, `disabled` or `reverted`. |
| `retrorun_fps_counter` | Enables the on-screen FPS counter. |
| `retrorun_show_loading_screen` | Shows the logo and loading message while a core initializes, for at least 700 ms. |
| `retrorun_ui_profile` | `auto`, `handheld` or `desktop`; default is `auto`. It can also be changed at runtime under **Settings > Video**. |
| `retrorun_video_filter` | `off`, `nearest` or `linear`; default is `off`. |
| `retrorun_video_shader` | `off`, `scanlines` or `crt`; default is `off`. |
| `retrorun_decorations` | `off` or `auto`; loads a matching local PNG or uses the generated fallback. |
| `retrorun_decorations_path` | Optional root directory for locally installed decoration sets. |
| `retrorun_decoration_source` | `auto`, `retrorun` or the identifier of an installed AmberELEC/ArkOS pack. Normally managed from the Decorations menu. |
| `retrorun_decoration_pack` | Identifier of the selected downloadable RetroRun/libretro pack. Normally managed from the Decorations menu. |
| `retrorun_loop_declared_fps` | Paces the frontend using the frame rate declared by the core. |
| `retrorun_adaptive_frameskip` | Enables adaptive presentation skipping; experimental and `false` by default. |
| `retrorun_frameskip` | Fixed number of video callbacks skipped after each presented frame, from `0` to `5`; default is `0` and takes precedence over adaptive frameskip. |
| `retrorun_video_renderer` | SDL2 only: `auto`, `software`, `opengl` or `vulkan`. Requires restart. Vulkan is not implemented yet. On Linux `opengl` means OpenGL ES. |
| `retrorun_vsync` | SDL2 only; default is `false` and can also be changed from the menu. |

General and input settings:

| Setting | Description |
| --- | --- |
| `retrorun_log_level` | `INFO`, `DEBUG`, `WARNING` or `ERROR`. |
| `retrorun_core_log_level` | Independent log level for libretro cores; defaults to `ERROR`. Set it explicitly to `INFO` or `DEBUG` for core diagnostics while keeping RetroRun's level independent. |
| `retrorun_log_to_file` | Duplicates RetroRun and libretro core logs to `retrorun.log` in the directory containing the active `retrorun.cfg` (for example `/storage/.config/distribution/configs/retrorun.log` on AmberELEC). The file is recreated at each launch; defaults to `false`. |
| `retrorun_device_name` | Overrides automatic device identification. |
| `retrorun_screenshot_folder` | Destination for screenshots. |
| `retrorun_audio_buffer` | Audio buffer value such as `-1`, `256`, `512` or `1024`. |
| `retrorun_audio_stable_buffer` | Optional extra audio buffering for difficult cores on SDL2 and GO2 (`false` by default). |
| `retrorun_force_audio_multithread` | Runs blocking audio submission on a dedicated bounded worker thread. It can help demanding cores, especially on RG552; defaults to `false`. |
| `retrorun_auto_save` | Saves automatically during shutdown. |
| `retrorun_auto_load` | Loads the automatic save at startup. |
| `retrorun_analog_to_digital` | Analog-to-D-pad mode: `none`, `left`, `right`, `left_forced` or `right_forced`. Non-forced modes are disabled automatically when the core requests native analog input. Defaults to `left_forced` for backward compatibility. |
| `retrorun_force_left_analog_stick` | Deprecated compatibility setting. `true` maps to `left_forced`, `false` maps to `none`; ignored when `retrorun_analog_to_digital` is present. |
| `retrorun_swap_l1r1_with_l2r2` | Exchanges shoulder buttons and triggers. |
| `retrorun_swap_sticks` | Exchanges left and right analog sticks. |
| `retrorun_alternative_input_mode` | Uses the ArkOS-style Select/F2 hotkeys. |
| `retrorun_mouse_speed_factor` | Mouse emulation speed; default is `5`. |
| `retrorun_force_video_multithread` | Legacy RG552 hint. Ignored with a warning on other devices, where detached presentation can be unsafe. |
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
retrorun_mapping_button_left = DPadLeft
retrorun_mapping_button_right = DPadRight
retrorun_mapping_button_a = A
retrorun_mapping_button_b = B
retrorun_mapping_button_x = SELECT
retrorun_mapping_button_y = Y
retrorun_mapping_button_select = SELECT
retrorun_mapping_button_start = START
retrorun_mapping_button_l1 = TopLeft
retrorun_mapping_button_r1 = TopRight
retrorun_mapping_button_l2 = TriggerLeft
retrorun_mapping_button_r2 = TriggerRight
retrorun_mapping_button_l3 = F1
retrorun_mapping_button_r3 = F2
retrorun_mapping_button_f1 = F1
retrorun_mapping_button_f2 = F2
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

### GitHub Actions binary builds

The **Build RetroRun binaries** workflow can be started manually from the
repository's **Actions** tab. It produces three downloadable artifacts:

- `retrorun-linux-go2-aarch64` for the native GO2/DRM backend;
- `retrorun-linux-sdl2-aarch64` for Linux handhelds using SDL2/KMSDRM;
- `retrorun-macos-sdl2-arm64` for Apple Silicon macOS.

The macOS and SDL2 Linux jobs run on GitHub-hosted Apple Silicon and ARM64
runners. The GO2 job runs on a standard GitHub Ubuntu runner but compiles
inside the AmberELEC build container, using the same RG351P ARM64 toolchain,
`librga` and target sysroot as the distribution. The initial GO2 build may take
longer while that environment is prepared; its toolchain, sources and compiler
cache are retained between runs to speed up later builds. No self-hosted runner
is required for this public repository.

When launching the workflow, select only the artifacts needed for the test.
After it completes, open the run summary and download the corresponding
artifact from **Artifacts**. Artifacts are kept for 30 days.

## History and credits

RetroRun was initially developed by OtherCrashOverride until 2020. Development
has been continued by navy1978 since 2021. This version integrates and extends
[libgo2](https://github.com/OtherCrashOverride/libgo2) and
[rg351p-js2box](https://github.com/lualiliu/RG351P_virtual-gamepad).

### Third-party libraries

- [rcheevos](https://github.com/RetroAchievements/rcheevos), integrated in
  `deps/rcheevos`, provides the RetroAchievements runtime and hashing client
  under the [MIT License](deps/rcheevos/LICENSE).
- [SDL2](https://www.libsdl.org/), [libpng](http://www.libpng.org/pub/png/libpng.html),
  [libcurl](https://curl.se/libcurl/) and [zlib](https://zlib.net/) provide the
  portable window, input, audio, PNG, network and compression facilities used
  by the SDL2 and macOS builds.
- The native GO2/DRM backend additionally uses Linux system libraries for
  [DRM](https://dri.freedesktop.org/libdrm/)/GBM, EGL/OpenGL ES, RGA, ALSA,
  [OpenAL Soft](https://openal-soft.org/) and
  [libevdev](https://www.freedesktop.org/wiki/Software/libevdev/).

The optional downloadable screen decorations are sourced from
[libretro/common-overlays](https://github.com/libretro/common-overlays) and
are licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
RetroRun records this attribution beside each downloaded artwork and does not
bundle the artwork in the source repository.

Thanks to Cebion, Christian_Haitian, dhwz, madcat1990, pkegg, superdealloc and
Szalik for their contributions and support.
