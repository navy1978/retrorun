# RetroRun for Anbernic Devices (RG351 M/P/V/MP, RG552, and RG503)

RetroRun is a libretro frontend designed specifically for Anbernic devices, including RG351 M/P/V/MP, RG552, and RG503. Enhance your gaming experience with RetroRun.

To use RetroRun you need ro tun first [rg351p-js2box](https://github.com/christianhaitian/RG351P_virtual-gamepad).
Better to create a bash file like the folliwong passing 3 parameters: core, rom, platform (this is just an example)

````
#!/bin/bash

. /etc/profile

echo 'starting retrorun emulator...'

CORE="$1"
ROM="${2##*/}"
PLATFORM="$3"

rm /dev/input/by-path/platform-odroidgo2-joypad-event-joystick || true
echo 'creating fake joypad'
./rg351p-js2xbox --silent -t oga_joypad &
#sleep 0.2
echo 'confguring inputs'


ln -s /dev/input/event3 /dev/input/by-path/platform-odroidgo2-joypad-event-joystick
chmod 777 /dev/input/by-path/platform-odroidgo2-joypad-event-joystick
echo 'using core:' "$1"
echo 'platform:' "$3"
echo 'starting game:' "$2"

FPS=''
GPIO_JOYPAD=''
sleep 0.2
echo 'using 64bit'
./retrorun_64_new --triggers $FPS $GPIO_JOYPAD -s /storage/roms/"$3" -d /roms/bios "$1" "$2"
sleep 0.5
rm /dev/input/by-path/platform-odroidgo2-joypad-event-joystick
kill $(pidof rg351p-js2xbox)
echo 'end!'
````

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
11. Working SaveState for all supported cores included flycast2021 and flycast
12. many more...

## RetroRun Configuration Parameters

These parameters can be set in the `retrorun.cfg` file:

---
Retrorun configuration parameters (to be set in the retrorun.cfg file)
======

**`retrorun_screenshot_folder`** = Defines the folder in which screenshots are saved. Default: /storage/roms/screenshots

**`retrorun_fps_counter`** = true | false 
- `true`: Display the FPS counter.
- `false`: Do not display the FPS counter. 
Default: false

**`retrorun_aspect_ratio`** = 2:1 | 4:3 | 5:4 | 16:9 | 16:10 | 1:1 | 3:2 | auto 
- `2:1`: Aspect ratio 2:1.
- `4:3`: Aspect ratio 4:3.
- `5:4`: Aspect ratio 5:4.
- `16:9`: Aspect ratio 16:9.
- `16:10`: Aspect ratio 16:10.
- `1:1`: Aspect ratio 1:1.
- `3:2`: Aspect ratio 3:2.
- `auto`: Automatically determine the aspect ratio. 
Default: provided by the core

**`retrorun_force_left_analog_stick`** = true | false 
- `true`: Force the left analog stick to act like the D-pad.
- `false`: Do not force the left analog stick to act like the D-pad.
Default: false

**`retrorun_auto_save`** = true | false 
- `true`: Enable auto-save.
- `false`: Disable auto-save.
Default: false

**`retrorun_auto_load`** = true | false 
- `true`: Enable auto-load.
- `false`: Disable auto-load.
Default: same as auto-save

**`retrorun_loop_60_fps`** = true | false 
- `true`: Restrict the game loop to not exceed 60 FPS.
- `false`: Do not restrict the game loop to 60 FPS.
Default: true

**`retrorun_audio_buffer`** = -1, 1, 256, 512, 1024, 2048, ... 
- `-1`: Audio buffer size linked to original FPS of the game.
- `1`, `256`, `512`, `1024`, `2048`, ...: Specific audio buffer size.
Default: -1

**`retrorun_swap_l1r1_with_l2r2`** = true | false 
- `true`: Swap the triggers L1 and L2 with R1 and R2.
- `false`: Do not swap the triggers L1 and L2 with R1 and R2.
Default: false

**`retrorun_swap_sticks`** = true | false 
- `true`: Swap the left analog stick with the right analog stick.
- `false`: Do not swap the left analog stick with the right analog stick.
Default: false

**`retrorun_tate_mode`** = auto | enabled | disabled | reverted 
- `auto`: Automatically enable tate mode for vertical games.
- `enabled`: Enable tate mode.
- `disabled`: Disable tate mode.
- `reverted`: Rotate the screen by 180 degrees.
Default: disabled

**`retrorun_log_level`** = INFO | DEBUG | WARNING | ERROR 
- `INFO`: Display informational logs.
- `DEBUG`: Display debug logs.
- `WARNING`: Display warning logs.
- `ERROR`: Display error logs.
Default: INFO

**`retrorun_device_name`** = 
- Sets the device name. If not found in the config, the device name will be detected differently.

**`retrorun_mouse_speed_factor`** = 
- Sets the speed factor for the mouse.
Default: 5

**`retrorun_toggle_osd_select_x`** = 
- Enable OSD with the combo select+x instead of the default L3+R3.
Default: false

**`retrorun_force_video_multithread`** = 
- Force execution of video task in another thread
Default: false (for RG552 is enabled by default)

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
