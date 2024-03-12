# retrorun for Anbernic Devices (RG351 M/P/V/MP, RG552 and RG503)
libretro frontend for Anbernic devices (RG351 M/P/V/MP, RG552 and RG503)\
Use this for RG351*/RG552 with rg351p-js2box available [here](https://github.com/christianhaitian/RG351P_virtual-gamepad).


## supported Cores (tested):

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

## This branch adds some features, the following are part of them:
1) External library libgo2 (https://github.com/OtherCrashOverride/libgo2) now is part of retrorun
2) Tate mode (useful for naomi and atomiswave games)
3) It enables right analog for ParallN64 (with C buttons management)
4) Show FPS in the logs is -f flag is passed as parameter
5) Show FPS on screen with button combination SELECT +Y
6) Configuration of cores is now done via file, by default the file is in: "/storage/.config/distribution/configs/retrorun.cfg" but another file can be specified via "-c" parameter
7) Manages different aspect ratio (as parameters)
8) Manages GPIO/USB joypad
9) OSD with explicative images
10) Info Menu with device / game information


Retrorun configuration parameters (to be set in the retrorun.cfg file)
======
1)  retrorun_screenshot_folder = <whatever>
    Define the folder in which the screenshots are saved. Default: /storage/roms/screenshots
2)  retrorun_fps_counter = true | false
    Set this to true to shoe the FPS counter. Default: false
3)  retrorun_aspect_ratio = 2:1 | 4:3 | 5:4 | 16:9 | 16:10 | 1:1 | 3:2 | auto")
    Set aspect ratio of the screen. Default: the one provided by the core
4)  retrorun_force_left_analog_stick = true | false
    Froce the left analog stick to act like the DPAD. Default: false
5) retrorun_auto_save = true | false
    Specify if the auto save should be true or false. Default: false
6)  retrorun_loop_60_fps = true | false
    If set to true force the game loop to dont run faster than 60 FPS. Default: true
7) retrorun_audio_buffer = -1, 1, 256, 512, 1024, 2048, ...
    1 => basically the buffer is emptied in each call
    from 256 to 2028 => we wait until the buffer reaches that limit before empty it
    -1 => in this case the limit is linked to the original FPS of the game (for 60 FPS the audio buffer is 1/60 * 44100 = 735)
8) retrorun_swap_l1r1_with_l2r2 = true, false
    if true swaps the thriggers L1 and L2 with L2 and R2
9) retrorun_swap_sticks= true, false
    if true swpas the left analog stick with the right analog stick
10) retrorun_tate_mode = auto, enabled, disabled , reverted
    enable the tate mode (vertical games) can be enabled , disabled , reverted (rotating the screen of 180 degrees) and auto. In case of auto retrorun will automatically enable
    tate mode for vertical games           



Build
======
```
git clone https://github.com/navy1978/retrorun
make
strip retrorun
```


Configuration file
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
