# Configuration

[Back to README](../README.md)

RetroRun reads a simple `key = value` configuration file.

The parser accepts Unix or Windows line endings, UTF-8 BOMs, blank lines,
comments beginning with `#` or `;`, inline comments after whitespace, and
single- or double-quoted values. Quotes are useful when a value contains `#`,
`;`, leading/trailing whitespace or escape sequences. Duplicate keys are
reported and the last value wins. Malformed entries and invalid typed values
produce a line-numbered warning or error instead of being silently accepted.

- Native GO2 default: `/storage/.config/distribution/configs/retrorun.cfg`.
- SDL2 default: `./retrorun.cfg` in the current working directory.
- Every backend: pass `-c /path/to/retrorun.cfg` to select another file.
  An explicit `-c` always takes precedence over a `retrorun.cfg` in the
  current working directory.


See [Core options](../doc/CORE_OPTIONS.md) for libretro configuration examples
and [Controls](../doc/CONTROLS.md) for button remapping.

Frontend video settings:

| Setting | Values and behaviour |
| --- | --- |
| `retrorun_aspect_ratio` | `auto`, `2:1`, `4:3`, `5:4`, `16:9`, `16:10`, `1:1` or `3:2`. |
| `retrorun_pixel_perfect` | `true` or `false`; default is disabled. |
| `retrorun_tate_mode` | `auto`, `enabled`, `disabled` or `reversed` (`reverted` remains accepted for compatibility). |
| `retrorun_fps_counter` | Enables the on-screen FPS counter with `true`, `enabled` or `1`. |
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
| `retrorun_drm_direct_scanout` | GO2/DRM only: `true` or `false`; default is `false`. Direct scanout bypasses the RGA copy for compatible hardware-rendered frames. Enable it only after testing the relevant device and core; some cores, including `parallel_n64`, may be incompatible. The DRM diagnostics page can test it temporarily without changing this setting. |
| `retrorun_video_renderer` | SDL2 only: `auto`, `software`, `opengl` or `vulkan`. Requires restart. Vulkan is not implemented yet. On Linux `opengl` means OpenGL ES. |
| `retrorun_vsync` | SDL2 only; default is `false` and can also be changed from the menu. |

General and input settings:

| Setting | Description |
| --- | --- |
| `retrorun_log_level` | `INFO`, `DEBUG`, `WARNING` or `ERROR`. |
| `retrorun_core_log_level` | Independent log level for libretro cores; defaults to `ERROR`. Set it explicitly to `INFO` or `DEBUG` for core diagnostics while keeping RetroRun's level independent. |
| `retrorun_log_to_file` | Duplicates RetroRun and libretro core logs to `retrorun.log` in the directory containing the active `retrorun.cfg` (for example `/storage/.config/distribution/configs/retrorun.log` on AmberELEC). The file is recreated at each launch; defaults to `false`. |
| `retrorun_device_name` | Overrides automatic device identification (for example, `Miniloong Pocket 1`). |
| `retrorun_screenshot_folder` | Destination for screenshots. |
| `retrorun_audio_buffer` | Audio buffer value such as `-1`, `256`, `512` or `1024`. |
| `retrorun_audio_stable_buffer` | Optional extra audio buffering for difficult cores on SDL2 and GO2 (`false` by default). |
| `retrorun_sdl_audio_stretch_percent` | SDL2-only low-watermark audio compensation, clamped to `0`–`10`; `0` disables it and is the default. The tested RG351V low-end value is `5`. |
| `retrorun_sdl_audio_stretch_low_ms` | SDL2 queue threshold for compensation, clamped to `0`–`200` ms; defaults to `40`. |
| `retrorun_go2_audio_stretch_percent` | GO2/OpenAL low-watermark audio compensation, clamped to `0`–`10`; `0` disables it and is the recommended default. The measured `3` percent profile remains experimental after an inconclusive manual listening test. |
| `retrorun_go2_audio_stretch_low_ms` | GO2/OpenAL queue threshold for compensation, clamped to `0`–`200` ms; defaults to `40`; the experimental RG351V profile used `20`. |
| `retrorun_go2_audio_prebuffer_ms` | GO2/OpenAL startup prebuffer, clamped to `0`–`200` ms; defaults to `60`. Values below twice `retrorun_audio_buffer` are raised to that safe minimum. |
| `retrorun_force_audio_multithread` | Runs blocking audio submission on a dedicated bounded worker thread. It can help demanding cores, especially on RG552; defaults to `false`. |
| `retrorun_auto_save` | Saves automatically during shutdown. |
| `retrorun_auto_load` | Loads the automatic save at startup. A missing first-run state is reported as the short informational message `No auto state`, not as an error. |
| `retrorun_analog_to_digital` | Analog-to-D-pad mode: `none`, `left`, `right`, `left_forced` or `right_forced`. Non-forced modes are disabled automatically when the core requests native analog input. Defaults to `left_forced` for backward compatibility. |
| `retrorun_force_left_analog_stick` | Deprecated compatibility setting. `true` maps to `left_forced`, `false` maps to `none`; ignored when `retrorun_analog_to_digital` is present. |
| `retrorun_swap_l1r1_with_l2r2` | Exchanges shoulder buttons and triggers. |
| `retrorun_swap_sticks` | Exchanges left and right analog sticks. |
| `retrorun_alternative_input_mode` | Uses the ArkOS-style Select/F2 hotkeys. |
| `retrorun_mouse_speed_factor` | Mouse emulation speed; default is `5`. |
| `retrorun_force_video_multithread` | Legacy boolean which requests threaded hardware-frame presentation on RG552 and the RG353 family. In the default `auto` mode, RG552 also retains its historical automatic Flycast 2021-family path. |
| `retrorun_video_multithread_mode` | Selects `auto` (backward-compatible behaviour), `enabled`, or `disabled`. Unlike the legacy boolean, `disabled` also turns off the automatic RG552/Flycast 2021-family path, allowing a real controlled A/B test. Unsupported devices still ignore threaded presentation. |
| `retrorun_flycast_game_profile` | Flycast-only Product-number catalog: `disabled` (default), `best_validated` or `best_performance`. A newer valid `flycast-game-catalog.ini` beside RetroRun overrides the built-in catalog. |
| `retrorun_flycast_catalog_update` | `auto` (default) or `disabled`. When a Flycast game profile is active, `auto` checks GitHub at most once per day in a separate process and atomically caches only a newer, valid catalog. |
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

## Runtime settings menu

The Settings menu writes user-facing choices back to the active
`retrorun.cfg`. Saves, controls, video, audio, performance,
RetroAchievements and diagnostics are kept in separate submenus. Settings
labelled `(restart)` are stored immediately but take effect when RetroRun or
the core is restarted. Backend-specific entries are only shown where they are
usable: SDL renderer and VSync on SDL2, DRM direct scanout on GO2/DRM, and
forced threaded video on RG552 and the RG353 family. Fixed and adaptive frameskip are
mutually exclusive.

The device information page also reports the detected SoC. On Rockchip
handhelds RetroRun reads the device-tree compatibility data first (for example
`Rockchip RK3326`, `Rockchip RK3566` or `Rockchip RK3399`) and only falls back
to its known-device table when that data is unavailable.

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
