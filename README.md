# RetroRun for Anbernic Devices (RG351 M/P/V/MP, RG552, and RG503)

RetroRun is a libretro frontend designed specifically for Anbernic devices, including RG351 M/P/V/MP, RG552, and RG503. Enhance your gaming experience with RetroRun.

To use RetroRun with RG351P, check out the available [rg351p-js2box](https://github.com/christianhaitian/RG351P_virtual-gamepad).

## Supported Cores (Tested)

- Flycast, Flycast 2021
- ParaLLEl N64
- SwanStation
- DuckStation
- PCSX-ReARMed
- mGBA
- VBA-M
- Virtual Jaguar
- DOSBox-pure
- DOSBox-core
- Beetle VB
- Yaba Sanshiro
- Snes9x, Snes9x 2002, Snes9x 2005 Plus, Snes9x 2010
- Genesis Plus GX
- PicoDrive
- PPSSPP
- Stella, Stella2014

## Key Features

This branch of RetroRun includes several features:

1. Integration of the external library libgo2 (https://github.com/OtherCrashOverride/libgo2) into RetroRun.
2. Tate mode support (useful for Naomi and Atomiswave games).
3. Right analog support for ParaLLEl N64, including C buttons management.
4. Option to display FPS in the logs using the -f flag.
5. Display FPS on the screen using the SELECT + Y button combination.
6. Core configuration via file, with default location at "/storage/.config/distribution/configs/retrorun.cfg", but customizable via the -c parameter.
7. Management of different aspect ratios.
8. Support for GPIO/USB joypad.
9. On-Screen Display (OSD) with explanatory images.
10. Info Menu displaying device and game information.

## RetroRun Configuration Parameters

These parameters can be set in the `retrorun.cfg` file:

---

**retrorun_screenshot_folder**: Defines the folder for saving screenshots. Default: `/storage/roms/screenshots`

**retrorun_fps_counter**: Enables/disables the FPS counter. Default: `false`

**retrorun_aspect_ratio**: Sets the screen aspect ratio. Default: provided by the core.

**retrorun_force_left_analog_stick**: Forces the left analog stick to act like the D-pad. Default: `false`

**retrorun_auto_save**: Specifies whether auto-save should be enabled. Default: `false`

**retrorun_auto_load**: Specifies whether auto-load should be enabled. Default: same as auto-save.

**retrorun_loop_60_fps**: Restricts the game loop to not exceed 60 FPS. Default: `true`

**retrorun_audio_buffer**: Specifies the audio buffer size. Default: `-1`

**retrorun_swap_l1r1_with_l2r2**: Swaps L1 and L2 triggers with R1 and R2. Default: `false`

**retrorun_swap_sticks**: Swaps the left analog stick with the right analog stick. Default: `false`

**retrorun_tate_mode**: Enables tate mode (vertical games). Default: `disabled`

**retrorun_log_level**: Sets the logger level. Default: `INFO`

**retrorun_device_name**: Sets the device name. If not found in config, the device name will be detected differently.

**retrorun_mouse_speed_factor**: Sets the speed factor for the mouse. Default: `5`

---

## Build

Clone the repository and build RetroRun:

```shell
git clone https://github.com/navy1978/retrorun
make
strip retrorun
```


## Configuration file
======

The configuration file looks like the following:
````
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

# ---- RETRORUN RUNTIME SETTINGS ----

````
