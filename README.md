# RetroRun for Anbernic Devices (RG351 M/P/V/MP, RG552, RG503, RG353M, and RG353V)

RetroRun is a libretro frontend specifically designed for Anbernic devices, including the RG351 M/P/V/MP, RG552, RG503, RG353M, and RG353V. It enhances your gaming experience with improved performance and simplified configuration.

This version of RetroRun differs significantly from retrorun-go2, as it encapsulates libgo2 and rg351p-js2box within itself. This eliminates the need for complex joypad configuration or compiling additional libraries. These libraries are not only integrated but also enhanced to provide the best possible experience.

Launching a Game with RetroRun

To launch a game using RetroRun, it is recommended to create a bash script (retrorun.sh) that accepts three parameters: core, rom, and platform (optional). Below is an example script:
````
#!/bin/bash

. /etc/profile
echo 'Starting RetroRun emulator...'
CORE="$1"
ROM="${2##*/}"
PLATFORM="$3" # Optional

FPS='-f' # Optional
GPIO_JOYPAD='-g' # Optional
./retrorun_64_new --triggers $FPS $GPIO_JOYPAD -s /storage/roms/"$3" -d /roms/bios "$1" "$2"
echo 'Emulation ended!'
````

To run the script:
````
./retrorun.sh /path/to/core /path/to/game name_of_platform
````


## Supported Cores (Tested)

RetroRun has been tested and supports the following cores:<br>
	•	<u>Dreamcast<\u>: Flycast, Flycast 2021<br>
	•	<u>Nintendo 64<\u>: ParaLLEl N64<br>
	•	<u>PlayStation<\u>: SwanStation, DuckStation, PCSX-ReARMed  <br>
	•	<u>Game Boy Advance<\u>: mGBA, VBA-M  <br>
	•	<u>Atari Jaguar<\u>: Virtual Jaguar  <br>
	•	<u>DOS Emulation<\u>: DOSBox-pure, DOSBox-core <br> 
	•	<u>Atari 2600<\u>: Stella, Stella 2014  <br>
	•	<u>Sega Consoles<\u>: Genesis Plus GX, PicoDrive  <br>
	•	<u>Super Nintendo<\u>: Snes9x, Snes9x 2002, Snes9x 2005 Plus, Snes9x 2010  <br>
	•	<u>Sony PSP<\u>: PPSSPP  <br>
	•	<u>Others<\u>: Beetle VB, Yaba Sanshiro <br> 

## Key Features

This project adds many improvements and new features to the retrorun-go2 project (https://github.com/OtherCrashOverride/retrorun-go2):  <br>
	1.	Integrated libgo2 library (https://github.com/OtherCrashOverride/libgo2)  <br>
    1.	Integrated rg351p-js2box library (https://github.com/lualiliu/RG351P_virtual-gamepad)  <br>
	2.	Tate Mode Support (useful for Naomi and Atomiswave vertical games)  <br>
	3.	Right Analog Support for ParaLLEl N64, including C-buttons management  <br>
	4.	FPS Display Options:  <br>
	•	Log FPS using the -f flag  <br>
	•	Display FPS on-screen with SELECT + Y  <br>
	5.	Core Configuration via File  <br>
	•	Default: /storage/.config/distribution/configs/retrorun.cfg  <br>
	•	Customizable with the -c parameter  <br>
	6.	Aspect Ratio Management  <br>
	7.	Support for GPIO/USB Joypads  <br>
	8.	On-Screen Display (OSD) with explanatory images  <br>
	9.	Info Menu displaying device and game information  <br>
	10.	Working SaveState support for all cores, including   Flycast2021 and Flycast  <br>
	11.	Rumble support for all cores and devices  <br>
	12.	Many additional enhancements  <br>

⸻

## RetroRun Configuration Parameters

These parameters can be set in the `retrorun.cfg` file:  

General Settings  
	•	**retrorun_screenshot_folder** = /storage/roms/screenshots (Default screenshot save folder)  <br>
	•	**retrorun_fps_counter** = true | false (Display FPS counter, default: false)  <br>
	•	**retrorun_aspect_ratio** = 2:1 | 4:3 | 5:4 | 16:9 | 16:10 | 1:1 | 3:2 | auto (Aspect ratio, default: core-defined)  <br>
	•	**retrorun_log_level** = INFO | DEBUG | WARNING | ERROR (Log level, default: INFO)  <br>

Input Settings  
	•	**retrorun_force_left_analog_stick** = true | false (Map left analog stick to D-pad, default: false)  <br>
	•	**retrorun_swap_l1r1_with_l2r2** = true | false (Swap L1/L2 with R1/R2, default: false)  <br>
	•	**retrorun_swap_sticks** = true | false (Swap left and right analog sticks, default: false)  <br>
	•	**retrorun_toggle_osd_select_x** = true | false (Enable OSD toggle with SELECT+X instead of L3+R3, default: false)  <br>

Save/Load Features  
	•	**retrorun_auto_save** = true | false (Enable auto-save, default: false)  <br>
	•	**retrorun_auto_load** = true | false (Enable auto-load, default: same as auto-save)  <br>

Performance Settings  
	•	**retrorun_loop_60_fps** = true | false (Restrict loop to 60 FPS, default: true)  <br>
	•	**retrorun_audio_buffer** = -1, 1, 256, 512, 1024, ... (Audio buffer size, default: -1)  <br>
	•	**retrorun_force_video_multithread** = true | false (Run video tasks in a separate thread, default: varies by device)  <br>

Tate Mode Settings  
	•	**retrorun_tate_mode** = auto | enabled | disabled | reverted (Enable vertical display mode, default: disabled)<br>

Rumble Settings  
	•	**`retrorun_rumble_type**` = pwm | event (Rumble type, default varies by device)  <br>
	•	**retrorun_rumble_event** = /dev/input/eventX (Override default rumble event file)  <br>
	•	**retrorun_rumble_pwm_file** = /sys/class/pwm/pwmchip0/pwm0/duty_cycle (Override default PWM file)  <br>

## Build

Clone the repository and compile RetroRun using:
```shell
git clone https://github.com/navy1978/retrorun
cd retrorun
make
strip retrorun
```


## Configuration file

The configuration file (retrorun.cfg) contains settings for different cores. Example:<br>
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

````
(*) Pay attention to the parameter names, as they follow the naming convention of the core. For example, in some distributions, the Flycast core is named Reicast. In such cases, parameters should be prefixed accordingly—e.g., 'flycast_threaded_rendering' should be renamed to 'reicast_threaded_rendering'.<br>


## Conclusion

RetroRun delivers a streamlined and optimized gaming experience for Anbernic devices, offering enhanced performance, simplified configuration, and broad core compatibility.  <br>

For more details or to contribute, visit the [GitHub repository](https://github.com/navy1978/retrorun).  <br>

### Development History  
RetroRun was initially developed by **OtherCrashOverride** until 2020. Since 2021, development has been continued and maintained by **navy1978**.  <br>

### Special Thanks  
A heartfelt thanks to:  <br>
**Cebion, Christian_Haitian, dhwz, madcat1990, and Szalik** for their contributions and support.  <br>