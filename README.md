# retrorun-go2 for RG351 M/P/V/MP and RG552
libretro frontend for RG351 M/P/V/MP and RG552\
Use this for RG351*/RG552 with rg351p-js2box available [here](https://github.com/christianhaitian/RG351P_virtual-gamepad).

# This branch adds the following features:
1) Tate mode (useful for naomi and atomiswave games)
2) It enables right analog for ParallN64 (with C buttons management)
3) Show FPS in the logs is -f flag is passed as parameter
4) Show FPS on screen with button combination SELECT +Y
5) Configuration of cores is now done via file, by default the file is in: "/storage/.config/distribution/configs/retrorun.cfg" but another file can be specified via "-c" parameter
6) Manages different aspect ratio (as parameters)
7) Manages GPIO/USB joypad


Retrorun parameters
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
7)  retrorun_video_another_thread = true | false | half
    Specify if process the call to video card in another thread. True means all the calls will be executed in another thread , false none, half means helf will be executed in the current thread and half in another thread. Default: true
8)  retrorun_video_another_thread_wait_millisec = <whatever>
    Only works if retrorun_video_another_thread is equals to true or half. It specify the number of milliseconds to wait before execute the video card call in anohter thread. Default: 0
9) retrorun_adaptive_fps = true | false
    When true it tries to recuce the number of calls to libretro run , it can help in case the number of FPS is too low. Default: false
10) retrorun_audio_buffer = -1, 1, 256, 512, 1024, 2048, ...
    1 => basically the buffer is emptied in each call
    from 256 to 2028 => we wait until the buffer reaches that limit before empty it
    -1 => in this case the limit is linked to the original FPS of the game (for 60 FPS the audio buffer is 1/60 * 44100 = 735)



Build
======
```
git clone https://github.com/navy1978/retrorun-go2
make
strip retrorun
```
