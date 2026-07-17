/*
retrorun - libretro frontend for Anbernic Devices
Copyright (C) 2020  OtherCrashOverride
Copyright (C) 2021-present  navy1978
*/

#include "menu_setup.h"
#include "globals.h"
#include "config.h"
#include "core_loader.h"
#include "savestate.h"
#include "video.h"
#include "video-helper.h"
#include "audio.h"
#include "input.h"
#include "rumble.h"
#include "platform.h"
#include "decoration.h"

#include "menu/menu_manager.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <limits>
#include <ctime>
#include <cstdlib>
#include <sys/stat.h>

// --- Constants ---

extern bool first_video_refresh;
extern const char *opt_savedir;
extern const char *arg_rom;
extern rr_brightness_state_t brightnessState;
extern bool isRunning;

// --- Aspect ratio data ---

std::map<float, int> aspectRatioMap = {
    {2.0f, 0},
    {1.333333f, 1},
    {1.25f, 2},
    {1.777777f, 3},
    {1.6f, 4},
    {1.0f, 5},
    {1.5f, 6},
    {game_aspect_ratio, 7}};

const char *aspect_ratio_names_array[] = {
    "2:1",
    "4:3",
    "5:4",
    "16:9",
    "16:10",
    "1:1",
    "3:2",
    "auto"};

// --- Simple callbacks ---

void fake(int)
{
}

void resume(int button)
{
    if (button == A_BUTTON)
    {
        input_info_requested = false;
    }
}

void showCredit(int button)
{
    if (button == A_BUTTON)
    {
        resetCredisPosition();
        input_credits_requested = true;
    }
    else if (button == B_BUTTON)
    {
        resetCredisPosition();
        input_credits_requested = false;
    }
}

std::chrono::steady_clock::time_point last_rumble_time;
int testRumble(int)
{
    auto now = std::chrono::steady_clock::now();

    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_rumble_time).count() < 1)
    {
        std::cout << "Rumble call ignored to prevent overlap.\n";
        return 0;
    }

    last_rumble_time = now;

    if (!retrorun_input_set_rumble(0, RETRO_RUMBLE_STRONG, 0xFFFF))
    {
        std::cerr << "Failed to start rumble\n";
        return 0;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    retrorun_input_set_rumble(0, RETRO_RUMBLE_STRONG, 0);
    return 1;
}

// --- Aspect ratio ---

int getClosestValue(float value)
{
    if (value <= 0.0f)
        return 7;
    int result = 1;
    aspectRatioMap = {
        {2.0f, 0},
        {1.333333f, 1},
        {1.25f, 2},
        {1.777777f, 3},
        {1.6f, 4},
        {1.0f, 5},
        {1.5f, 6},
        {1.333333f, 7}};

    float minDifference = std::numeric_limits<float>::max();
    for (const auto &pair : aspectRatioMap)
    {
        float difference = std::abs(pair.first - value);
        if (difference < minDifference)
        {
            minDifference = difference;
            result = pair.second;
        }
    }
    return result;
}

int getAspectRatioSettings()
{
    return opt_aspect == 0.0f ? 7 : getClosestValue(opt_aspect);
}

std::function<void(int)> setAspectRatioSettings = [](int button)
{
    const int aspect_ratio_count = sizeof(aspect_ratio_names_array) / sizeof(aspect_ratio_names_array[0]);
    int currentIndex = getAspectRatioSettings();

    if (button == RIGHT)
        currentIndex = currentIndex + 1;
    else if (button == LEFT)
        currentIndex = currentIndex - 1;

    if (currentIndex < 0)
        currentIndex = aspect_ratio_count - 1;
    else if (currentIndex >= aspect_ratio_count)
        currentIndex = 0;

    opt_aspect = getAspectRatio(aspect_ratio_names_array[currentIndex]);
    aspect_ratio = opt_aspect == 0.0f ? game_aspect_ratio : opt_aspect;
    prepareScreen(currentWidth, currentHeight);
    persistVideoSetting("retrorun_aspect_ratio", aspect_ratio_names_array[currentIndex]);
};

// --- Tate mode ---

int getTateMode()
{
    return (int)tateState;
}

std::function<void(int)> setTateMode = [](int button)
{
    if (button == RIGHT)
        tateState = static_cast<TateState>((tateState + 1) % (AUTO + 1));
    else if (button == LEFT)
        tateState = static_cast<TateState>((tateState - 1) < DISABLED ? AUTO : (tateState - 1));
    first_video_refresh = true;
};

// --- Swap triggers/sticks ---

int getSwapTriggers()
{
    return swapL1R1WithL2R2 ? 1 : 0;
}

std::function<void(int)> setSwapTriggers = [](int button)
{
    if (button == LEFT || button == RIGHT)
        swapL1R1WithL2R2 = !swapL1R1WithL2R2;
};

int getSwapSticks()
{
    return swapSticks ? 1 : 0;
}

std::function<void(int)> setSwapSticks = [](int button)
{
    if (button == LEFT || button == RIGHT)
        swapSticks = !swapSticks;
};

// --- Lock FPS ---

int getLockDeclaredFPS()
{
    return runLoopAtDeclaredfps ? 1 : 0;
}

std::function<void(int)> setLockDeclaredFPS = [](int button)
{
    if (button == LEFT || button == RIGHT)
        runLoopAtDeclaredfps = !runLoopAtDeclaredfps;
};

// --- persistVideoSetting is in config.cpp ---

// --- SDL video renderer ---

#ifdef RR_PLATFORM_SDL
int getSDLVideoRenderer()
{
    return static_cast<int>(sdlVideoRenderer);
}

std::function<void(int)> setSDLVideoRenderer = [](int button)
{
    static const char *names[] = {"auto", "software", "opengl", "vulkan"};
    int renderer = static_cast<int>(sdlVideoRenderer);
    if (button == LEFT) renderer = (renderer + 3) % 4;
    if (button == RIGHT) renderer = (renderer + 1) % 4;
    sdlVideoRenderer = static_cast<SDLVideoRenderer>(renderer);
    if (persistVideoSetting("retrorun_video_renderer", names[renderer]))
        logger.log(Logger::WARN, "Video renderer saved; restart RetroRun to apply it");
    else
        logger.log(Logger::ERR, "Unable to save video renderer in '%s'", activeConfigFile.c_str());
};

int getSDLVsync()
{
    return sdlVsync ? 1 : 0;
}

std::function<void(int)> setSDLVsync = [](int button)
{
    if (button != LEFT && button != RIGHT)
        return;

    sdlVsync = !sdlVsync;
    const bool applied = rr_video_vsync_set(sdlVsync);
    if (!persistVideoSetting("retrorun_vsync", sdlVsync ? "true" : "false"))
        logger.log(Logger::ERR, "Unable to save VSync in '%s'", activeConfigFile.c_str());
    if (!applied)
        logger.log(Logger::WARN, "SDL could not apply VSync on the active renderer");
    else
        logger.log(Logger::DEB, "VSync %s", sdlVsync ? "enabled" : "disabled");
};
#endif

// --- Video filter ---

int getDecorationSetting()
{
    return decoration_enabled() ? 1 : 0;
}

std::function<void(int)> setDecorationSetting = [](int button)
{
    if (button == LEFT || button == RIGHT)
        decoration_set_enabled(!decoration_enabled());
};

int getVideoFilter()
{
    return static_cast<int>(videoFilter);
}

std::function<void(int)> setVideoFilter = [](int button)
{
    if (button != LEFT && button != RIGHT)
        return;
    int value = static_cast<int>(videoFilter);
    if (button == LEFT) value = (value + 2) % 3;
    if (button == RIGHT) value = (value + 1) % 3;
    videoFilter = static_cast<rr_video_filter_t>(value);
    rr_video_filter_set(videoFilter);
    static const char *names[] = {"off", "nearest", "linear"};
    if (!persistVideoSetting("retrorun_video_filter", names[value]))
        logger.log(Logger::ERR, "Unable to save video filter in '%s'", activeConfigFile.c_str());
};

// --- UI profile ---

int getUIProfileSetting()
{
    return static_cast<int>(getUIProfile());
}

std::function<void(int)> setUIProfileSetting = [](int button)
{
    if (button != LEFT && button != RIGHT)
        return;
    int value = static_cast<int>(getUIProfile());
    if (button == LEFT) value = (value + 2) % 3;
    if (button == RIGHT) value = (value + 1) % 3;
    setUIProfile(static_cast<UIProfile>(value));
    static const char *names[] = {"auto", "handheld", "desktop"};
    if (!persistVideoSetting("retrorun_ui_profile", names[value]))
        logger.log(Logger::ERR, "Unable to save UI profile in '%s'", activeConfigFile.c_str());
};

// --- Video shader ---

int getVideoShader()
{
    return static_cast<int>(videoShader);
}

std::function<void(int)> setVideoShader = [](int button)
{
    if (button != LEFT && button != RIGHT)
        return;
    int value = static_cast<int>(videoShader);
    if (button == LEFT) value = (value + 2) % 3;
    if (button == RIGHT) value = (value + 1) % 3;
    videoShader = static_cast<rr_video_shader_t>(value);
    rr_video_shader_set(videoShader);
    static const char *names[] = {"off", "scanlines", "crt"};
    if (!persistVideoSetting("retrorun_video_shader", names[value]))
        logger.log(Logger::ERR, "Unable to save video shader in '%s'", activeConfigFile.c_str());
    logger.log(Logger::WARN, "Video shader saved; restart RetroRun to apply it on every backend");
};

// --- Pixel perfect ---

int getPixelPerfect()
{
    return pixel_perfect ? 1 : 0;
}

std::function<void(int)> setPixelPerfect = [](int button)
{
    if (button == LEFT || button == RIGHT)
    {
        // Decorations require an integer-scaled viewport. Keep the effective
        // setting enabled until the decoration is turned off, at which point
        // decoration_set_enabled() restores the user's previous preference.
        if (decoration_enabled())
            return;
        pixel_perfect = !pixel_perfect;
        prepareScreen(currentWidth, currentHeight);
        persistVideoSetting("retrorun_pixel_perfect", pixel_perfect ? "true" : "false");
    }
};

// --- Audio disabled ---

int getAudioDisabled()
{
    return audio_disabled ? 1 : 0;
}

std::function<void(int)> setAudioDisabled = [](int button)
{
    if (button == LEFT || button == RIGHT)
        audio_disabled = !audio_disabled;
};

// --- Rumble disabled ---

int getRumbleDisabled()
{
    return disableRumble ? 1 : 0;
}

std::function<void(int)> setRumbleDisabled = [](int button)
{
    if (button == LEFT || button == RIGHT)
        disableRumble = !disableRumble;
};

// --- Audio buffer ---

static int audio_buffer_array[] = {-1, 1, 256, 512, 1024, 2048, 4096};

int getAudioBuffer()
{
    return new_retrorun_audio_buffer;
}

std::function<void(int)> setAudioBuffer = [](int button)
{
    int audio_buffer_array_size = sizeof(audio_buffer_array) / sizeof(audio_buffer_array[0]);
    int current_index = -1;
    for (int i = 0; i < audio_buffer_array_size; ++i)
    {
        if (new_retrorun_audio_buffer == audio_buffer_array[i])
        {
            current_index = i;
            break;
        }
    }
    int new_index = -1;
    if (button == RIGHT)
        new_index = (current_index + 1) % audio_buffer_array_size;
    else if (button == LEFT)
        new_index = (current_index - 1 + audio_buffer_array_size) % audio_buffer_array_size;
    if (new_index >= 0)
        new_retrorun_audio_buffer = audio_buffer_array[new_index];
};

// --- Brightness ---

int getBrightnessValue()
{
    return brightnessState.level;
}

static int step_left_right = 10;

std::function<void(int)> setBrightnessValue = [](int button)
{
    int selectedBrightness = brightnessState.level;
    if (button == LEFT)
    {
        selectedBrightness -= step_left_right;
        if (selectedBrightness < 1) selectedBrightness = step_left_right;
        if (selectedBrightness > 100) selectedBrightness = 100;
        rr_input_brightness_write(selectedBrightness);
    }
    else if (button == RIGHT)
    {
        selectedBrightness += step_left_right;
        if (selectedBrightness < 1) selectedBrightness = step_left_right;
        if (selectedBrightness > 100) selectedBrightness = 100;
        rr_input_brightness_write(selectedBrightness);
    }
};

// --- Audio value (volume) ---

int getAudioValue()
{
    return getVolume();
}

std::function<void(int)> setAudioValue = [](int button)
{
    int selectedVolume = getVolume();
    if (button == LEFT)
    {
        selectedVolume -= step_left_right;
        if (selectedVolume < 0) selectedVolume = 0;
        if (selectedVolume > 100) selectedVolume = 100;
        setVolume(selectedVolume);
    }
    else if (button == RIGHT)
    {
        selectedVolume += step_left_right;
        if (selectedVolume < 0) selectedVolume = 0;
        if (selectedVolume > 100) selectedVolume = 100;
        setVolume(selectedVolume);
    }
};

// --- Device type ---

int getDeviceType()
{
    return deviceTypeSelected;
}

std::function<void(int)> setDeviceType = [](int button)
{
    auto it = controllerMap.find(deviceTypeSelected);
    if (it != controllerMap.end())
    {
        if (button == LEFT)
        {
            if (it == controllerMap.begin())
                it = controllerMap.end();
            deviceTypeSelected = (--it)->first;
        }
        else if (button == RIGHT)
        {
            if (++it == controllerMap.end())
                it = controllerMap.begin();
            deviceTypeSelected = it->first;
        }
    }
    g_retro.retro_set_controller_port_device(0, deviceTypeSelected);
};

// --- Save/Load slot ---

std::string getSlotNameStr(int slotNumber, std::string)
{
    std::string result = "Empty";
    char *savePath = createSavePath(arg_rom, opt_savedir);
    std::string savePath1 = savePath;
    savePath1 += slotNumber == 1 ? "" : "" + std::to_string(slotNumber - 1);
    if (fileExists(savePath1.c_str()))
    {
        struct stat info = {};
        if (stat(savePath1.c_str(), &info) == 0) {
            char date[32] = {};
            const std::tm* modified = std::localtime(&info.st_mtime);
            if (modified) std::strftime(date, sizeof(date), "%d %b %H:%M", modified);
            result = date;
        }
    }
    std::free(savePath);
    return "Slot " + std::to_string(slotNumber) + "  " + result;
}

void loadSaveSlotWrapper(int button, int slotNumber, std::string type)
{
    // Inline the slot logic
    if (button == A_BUTTON)
    {
        logger.log(Logger::DEB, "Slot number :%d\n", slotNumber);
        char *savePath = createSavePath(arg_rom, opt_savedir);
        std::string savePath1 = savePath;
        savePath1 += slotNumber == 1 ? "" : "" + std::to_string(slotNumber - 1);

        if (type == "Load")
        {
            logger.log(Logger::DEB, "loading file :%s\n", savePath1.c_str());
            int loaded = LoadState(savePath1.c_str());
            lastLoadSaveStateDoneOk = (loaded >= 0);
            input_slot_memory_load_done = true;
            lastLoadSaveStateDoneTime = (double)time(NULL);
        }
        else
        {
            logger.log(Logger::DEB, "saving file :%s\n", savePath1.c_str());
            SaveState(savePath1.c_str());
        }
        free(savePath);
    }

    input_info_requested = false;
}

void restartCore(int button)
{
    if (button == A_BUTTON)
    {
        lastLoadSaveStateDoneTime = (double)time(NULL);
        input_info_requested = false;
        input_slot_memory_reset_done = true;
        menuManager.resetMenu();
        g_retro.retro_reset();
    }
}
