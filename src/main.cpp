/*
retrorun - libretro frontend for Anbernic Devices
Copyright (C) 2020  OtherCrashOverride
Copyright (C) 2021-present  navy1978

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "globals.h"
#include "config.h"
#include "core_loader.h"
#include "savestate.h"
#include "menu_setup.h"
#include "video-helper.h"
#include "video.h"
#include "audio.h"
#include "input.h"
#include "rumble.h"
#include "platform.h"
#include "keyboard.h"
#include "file_browser.h"
#include "achievements.h"
#include "network_status.h"
#include "decoration.h"
#include "decoration_catalog.h"
#include "menu/menu.h"
#include "menu/menu_item.h"
#include "menu/menu_manager.h"

#ifdef RR_PLATFORM_SDL
#include <SDL.h>
#endif

#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <getopt.h>
#include <signal.h>
#include <pthread.h>
#include <chrono>
#include <thread>
#include <dlfcn.h>

#include "libretro.h"

using namespace std::chrono;

// --- Variables owned by main ---

const char *opt_savedir = ".";
const char *opt_systemdir = ".";
int opt_backlight = -1;
int opt_volume = -1;
bool opt_restart = false;
const char *arg_core = "";
const char *arg_rom = "";
int exitFlag = -1;
bool isRunning = true;
pthread_t main_thread_id;

extern bool first_video_refresh;
extern rr_brightness_state_t brightnessState;

// --- Command line options ---

static struct option longopts[] = {
    {"savedir", required_argument, NULL, 's'},
    {"systemdir", required_argument, NULL, 'd'},
    {"aspect", required_argument, NULL, 'a'},
    {"volume", required_argument, NULL, 'v'},
    {"backlight", required_argument, NULL, 'b'},
    {"restart", no_argument, NULL, 'r'},
    {"triggers", no_argument, NULL, 't'},
    {"analog", no_argument, NULL, 'n'},
    {"fps", no_argument, NULL, 'f'},
    {0, 0, 0, 0}};

// --- Main ---

int main(int argc, char *argv[])
{
    main_thread_id = pthread_self();
    printf("\n");
    printf("########### RETRORUN %s ###########\n", release.c_str());
    printf("libretro frontend for Anbernic Devices\n");
    printf("Copyright (C) 2020  OtherCrashOverride\n");
    printf("Copyright (C) 2021-present  navy1978\n");
    printf("\n");

    int c;
    int option_index = 0;

    while ((c = getopt_long(argc, argv, "s:d:a:b:v:grtnfc:", longopts, &option_index)) != -1)
    {
        switch (c)
        {
        case 's':
            opt_savedir = optarg;
            break;
        case 'd':
            opt_systemdir = optarg;
            break;
        case 'a':
            opt_aspect = atof(optarg);
            break;
        case 'b':
            opt_backlight = atoi(optarg);
            break;
        case 'v':
            opt_volume = atoi(optarg);
            break;
        case 'r':
            opt_restart = true;
            break;
        case 't':
            opt_triggers = true;
            break;
        case 'n':
            force_left_analog_stick = false;
            logger.log(Logger::INF, "using '-n' as parameter, forces left analog stick to false!.");
            break;
        case 'f':
            opt_show_fps = true;
            break;
        case 'g':
            gpio_joypad = true;
            break;
        case 'c':
            opt_setting_file = optarg;
            break;
        default:
            logger.log(Logger::ERR, "Unknown option. '%s'", longopts[option_index].name);
            exit(EXIT_FAILURE);
        }
    }

    getDeviceName();
    initConfig();

    if (!gpio_joypad)
    {
        if (isRG351MP() || isRG552())
            gpio_joypad = true;
    }

    int remaining_args = argc - optind;
    int remaining_index = optind;
    logger.log(Logger::DEB, "remaining_args=%d", remaining_args);

    if (remaining_args < 2)
    {
        logger.log(Logger::ERR, "Usage: %s [-s savedir] [-d systemdir] [-a aspect] core rom", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (optind < argc)
    {
        logger.log(Logger::DEB, "non-option ARGV-elements:");
        while (optind < argc)
            logger.log(Logger::DEB, " - %s ", argv[optind++]);
    }

    arg_core = argv[remaining_index++];
    arg_rom = argv[remaining_index++];

    rr_platform_preinit();
    input_gamepad_read();

    core_load(arg_core);

    if (isSwanStation() && (isRG351V() || isRG351MP()))
        opt_aspect = 0.75f;

    core_load_game(arg_rom);
    achievements_init(arg_rom);
    decoration_init(arg_rom);
    decoration_catalog_init();
    network_status_refresh();

    rr_input_state_t *gamepadState = input_gampad_current_get();
    if (rr_input_state_button_get(gamepadState, RRInputButton_F1) == RRButtonState_Pressed)
    {
        logger.log(Logger::WARN, "Forcing restart due to button press (F1)...");
        opt_restart = true;
    }

    // State paths
    char *sramPath = createSramPath(arg_rom, opt_savedir);
    char *savePath = createSavePath(arg_rom, opt_savedir);

    logger.log(Logger::DEB, "savePath='%s'", savePath);
    logger.log(Logger::DEB, "sramPath='%s'", sramPath);

    if (opt_restart)
    {
        logger.log(Logger::WARN, "Restarting...");
    }
    else
    {
        if (auto_load)
        {
            input_message = true;
            status_message = "Loading saved game...";
            logger.log(Logger::DEB, "Loading saved state - File '%s'", savePath);

            if (isParalleln64() || isDosBox() || isFlycast2021())
            {
                sleep(1);
                g_retro.retro_run();
            }

            LoadState(savePath);
            sleep(3);
        }
    }

    logger.log(Logger::DEB, "Loading sram - File '%s'", sramPath);
    LoadSram(sramPath);
    logger.log(Logger::DEB, "Entering render loop.");

    double elapsed = 0;
    int totalFrames = 0;

    struct retro_system_av_info info;
    g_retro.retro_get_system_av_info(&info);
    logger.log(Logger::DEB, "System Info - aspect_ratio: %f", info.geometry.aspect_ratio);
    logger.log(Logger::DEB, "System Info - base_width: %d", info.geometry.base_width);
    logger.log(Logger::DEB, "System Info - base_height: %d", info.geometry.base_height);
    logger.log(Logger::DEB, "System Info - max_width: %d", info.geometry.max_width);
    logger.log(Logger::DEB, "System Info - max_height: %d", info.geometry.max_height);
    logger.log(Logger::DEB, "System Info - fps: %f", info.timing.fps);
    logger.log(Logger::DEB, "System Info - sample_rate: %f", info.timing.sample_rate);

    auto prevClock = high_resolution_clock::now();
    auto totClock = high_resolution_clock::now();
    double max_fps = info.timing.fps;
    double previous_fps = 0;
    originalFps = info.timing.fps;
    if (max_fps < 1) max_fps = 60;
    if (originalFps < 1) originalFps = 60;

    bool redrawInfo = true;
    g_retro.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);

    if (isFlycast())
    {
        g_retro.retro_set_controller_port_device(1, 0);
        g_retro.retro_set_controller_port_device(2, 0);
        g_retro.retro_set_controller_port_device(3, 0);
    }

    unsigned long long countNumFps = 0;
    unsigned long long countValFps = 0;
    auto start_time = steady_clock::now();
    bool startCalAvgFps = false;

    // --- Menu construction ---

    MenuItem menuItem_slot1Load = MenuItem("Are you sure?", [](int arg) { loadSaveSlotWrapper(arg, 1, "Load"); });
    menuItem_slot1Load.setQuestionItem();
    std::vector<MenuItem> slot1Load_sure = {menuItem_slot1Load};
    Menu menuInfoSlot1Load = Menu("Slot1", slot1Load_sure);

    MenuItem menuItem_slot2Load = MenuItem("Are you sure?", [](int arg) { loadSaveSlotWrapper(arg, 2, "Load"); });
    menuItem_slot2Load.setQuestionItem();
    std::vector<MenuItem> slot2Load_sure = {menuItem_slot2Load};
    Menu menuInfoSlot2Load = Menu("Slot2", slot2Load_sure);

    MenuItem menuItem_slot3Load = MenuItem("Are you sure?", [](int arg) { loadSaveSlotWrapper(arg, 3, "Load"); });
    menuItem_slot3Load.setQuestionItem();
    std::vector<MenuItem> slot3Load_sure = {menuItem_slot3Load};
    Menu menuInfoSlot3Load = Menu("Slot3", slot3Load_sure);

    std::vector<MenuItem> itemsLoadStateLoad = {
        MenuItem([]() { return getSlotNameStr(1, "Load"); }, &menuInfoSlot1Load, fake),
        MenuItem([]() { return getSlotNameStr(2, "Load"); }, &menuInfoSlot2Load, fake),
        MenuItem([]() { return getSlotNameStr(3, "Load"); }, &menuInfoSlot3Load, fake)};
    Menu menuLoadState = Menu("Load State", itemsLoadStateLoad);

    MenuItem menuItem_slot1Save = MenuItem("Are you sure?", [](int arg) { loadSaveSlotWrapper(arg, 1, "Save"); });
    menuItem_slot1Save.setQuestionItem();
    std::vector<MenuItem> slot1Save_sure = {menuItem_slot1Save};
    Menu menuInfoSlot1Save = Menu("Slot1", slot1Save_sure);

    MenuItem menuItem_slot2Save = MenuItem("Are you sure?", [](int arg) { loadSaveSlotWrapper(arg, 2, "Save"); });
    menuItem_slot2Save.setQuestionItem();
    std::vector<MenuItem> slot2Save_sure = {menuItem_slot2Save};
    Menu menuInfoSlot2Save = Menu("Slot2", slot2Save_sure);

    MenuItem menuItem_slot3Save = MenuItem("Are you sure?", [](int arg) { loadSaveSlotWrapper(arg, 3, "Save"); });
    menuItem_slot3Save.setQuestionItem();
    std::vector<MenuItem> slot3Save_sure = {menuItem_slot3Save};
    Menu menuInfoSlot3Save = Menu("Slot3", slot3Save_sure);

    std::vector<MenuItem> itemsLoadStateSave = {
        MenuItem([]() { return getSlotNameStr(1, "Save"); }, &menuInfoSlot1Save, fake),
        MenuItem([]() { return getSlotNameStr(2, "Save"); }, &menuInfoSlot2Save, fake),
        MenuItem([]() { return getSlotNameStr(3, "Save"); }, &menuInfoSlot3Save, fake)};
    Menu menuSaveState = Menu("Save State", itemsLoadStateSave);

    std::vector<MenuItem> itemsState = {
        MenuItem("Load state", &menuLoadState, fake),
        MenuItem("Save state", &menuSaveState, fake)};
    Menu menuState = Menu("State", itemsState);

    std::vector<MenuItem> itemsSystem = {
        MenuItem("Volume", getAudioValue, setAudioValue, "%"),
        MenuItem("Brightness", getBrightnessValue, setBrightnessValue, "%")};
    Menu menuSystem = Menu("System", itemsSystem);

    MenuItem deviceType("Device type", getDeviceType, setDeviceType, "device-type");
    deviceType.setPossibleValues(controllerMap);

    std::vector<MenuItem> itemsControl = {
        deviceType,
        MenuItem("Swap triggers", getSwapTriggers, setSwapTriggers, "bool"),
        MenuItem("Swap analog sticks", getSwapSticks, setSwapSticks, "bool"),
        MenuItem("Rumble test", []() { return 0; }, [](int val) { testRumble(val); }, "test-rumble"),
        MenuItem("Rumble disabled", getRumbleDisabled, setRumbleDisabled, "bool")};
    Menu menuControl = Menu("Control", itemsControl);

    std::vector<MenuItem> itemsAudio = {
        MenuItem("Audio buffer", getAudioBuffer, setAudioBuffer, ""),
        MenuItem("Audio disabled", getAudioDisabled, setAudioDisabled, "bool")};
    Menu menuAudio = Menu("Audio", itemsAudio);

    std::vector<MenuItem> itemsAchievements = {
        MenuItem(achievements_status_label, [](int) {}),
        MenuItem(achievements_username_label,
                 [](int button) { if (button == A_BUTTON) achievements_edit_username(arg_rom); }),
        MenuItem("Set password",
                 [](int button) { if (button == A_BUTTON) achievements_edit_password(arg_rom); }),
        MenuItem("Enabled",
                 []() { return achievements_enabled() ? 1 : 0; },
                 [](int value) { achievements_set_enabled(value != 0, arg_rom); },
                 "bool")};
    Menu menuAchievements = Menu("RetroAchievements", itemsAchievements);

    MenuItem removeDecorationQuestion("Are you sure?", [](int button) {
        if (button == A_BUTTON) decoration_catalog_remove();
    });
    removeDecorationQuestion.setQuestionItem();
    std::vector<MenuItem> removeDecorationItems = {removeDecorationQuestion};
    Menu menuRemoveDecoration = Menu("Remove decoration", removeDecorationItems);

    std::vector<MenuItem> itemsDecorations = {
        MenuItem("Enabled", getDecorationSetting, setDecorationSetting, "decoration"),
        MenuItem(decoration_catalog_source_label, decoration_catalog_select_source),
        MenuItem(decoration_catalog_active_label, [](int) {}),
        MenuItem(decoration_catalog_pack_label, decoration_catalog_select),
        MenuItem(decoration_catalog_status_label, [](int) {}),
        MenuItem("Download / update", [](int button) {
            if (button == A_BUTTON) decoration_catalog_install();
        }),
        MenuItem("Remove RetroRun pack", &menuRemoveDecoration, fake),
        MenuItem("Downloads: libretro", [](int) {}),
        MenuItem("License: CC BY 4.0", [](int) {})};
    Menu menuDecorations = Menu("Decorations", itemsDecorations);

    std::vector<MenuItem> itemsVideo = {
        MenuItem("Aspect ratio", getAspectRatioSettings, setAspectRatioSettings, "aspect-ratio"),
        MenuItem("Pixel perfect", getPixelPerfect, setPixelPerfect, "bool"),
        MenuItem("Lock FPS", getLockDeclaredFPS, setLockDeclaredFPS, "bool"),
        MenuItem("UI profile", getUIProfileSetting, setUIProfileSetting, "ui-profile"),
        MenuItem("Decorations", &menuDecorations, fake),
#ifdef RR_PLATFORM_SDL
        MenuItem("Renderer (restart)", getSDLVideoRenderer, setSDLVideoRenderer, "video-renderer"),
        MenuItem("VSync", getSDLVsync, setSDLVsync, "bool"),
#endif
        MenuItem("Video filter", getVideoFilter, setVideoFilter, "video-filter"),
        MenuItem("Shader (restart)", getVideoShader, setVideoShader, "video-shader"),
        MenuItem("Tate mode", getTateMode, setTateMode, "rotation")};

    MenuItem menuItem_restart_core = MenuItem("Are you sure?", [](int arg) { restartCore(arg); });
    menuItem_restart_core.setQuestionItem();
    std::vector<MenuItem> restartCore_sure = {menuItem_restart_core};
    Menu menuResetCore = Menu("Reset Core", restartCore_sure);

    Menu menuVideo = Menu("Video", itemsVideo);

    std::vector<MenuItem> itemsSettings = {
        MenuItem("System", &menuSystem, fake),
        MenuItem("Control", &menuControl, fake),
        MenuItem("Video", &menuVideo, fake),
        MenuItem("Audio", &menuAudio, fake),
        MenuItem("RetroAchievements", &menuAchievements, fake),
        MenuItem("Reset Core", &menuResetCore, fake)};
    Menu menuSettings = Menu("Settings", itemsSettings);

    std::vector<MenuItem> device = {MenuItem(SHOW_DEVICE, NULL)};
    Menu menuInfoDevice = Menu("Device", device);
    std::vector<MenuItem> core = {MenuItem(SHOW_CORE, NULL)};
    Menu menuInfoCore = Menu("Libretro core", core);
    std::vector<MenuItem> game = {MenuItem(SHOW_GAME, NULL)};
    Menu menuInfoGame = Menu("Current game", game);
    std::vector<MenuItem> graphics = {MenuItem(SHOW_GRAPHICS, NULL)};
    Menu menuInfoGraphics = Menu("Graphics", graphics);
    std::vector<MenuItem> network = {
        MenuItem(network_status_connection_label, [](int) {}),
        MenuItem(network_status_interface_label, [](int) {}),
        MenuItem(network_status_address_label, [](int) {}),
        MenuItem(network_status_latency_label, [](int) {}),
        MenuItem(network_status_checked_label, [](int) {}),
        MenuItem("Refresh", [](int button) { if (button == A_BUTTON) network_status_refresh(); })};
    Menu menuInfoNetwork = Menu("Network", network);

    std::vector<MenuItem> itemsInfo = {
        MenuItem("Device", &menuInfoDevice, fake),
        MenuItem("Libretro core", &menuInfoCore, fake),
        MenuItem("Current game", &menuInfoGame, fake),
        MenuItem("Graphics", &menuInfoGraphics, fake),
        MenuItem("Network", &menuInfoNetwork, fake)};

    MenuItem menuItem_q = MenuItem("Are you sure?", [](int button) {
        if (button == A_BUTTON) { isRunning = false; }
    });
    menuItem_q.setQuitItem();
    std::vector<MenuItem> quit_sure = {menuItem_q};
    Menu menuInfoQuit = Menu("Quit", quit_sure);

    Menu menuInfo = Menu("Info", itemsInfo);
    std::vector<MenuItem> items = {
        MenuItem("Resume", resume),
        MenuItem("Change disk", [](int button) { if (button == A_BUTTON) rr_file_browser_open(arg_rom); }),
        MenuItem("Achievements", [](int button) { if (button == A_BUTTON) achievements_view_open(); }),
        MenuItem("Info", &menuInfo, fake),
        MenuItem("Settings", &menuSettings, fake),
        MenuItem("Load/Save", &menuState, fake),
        MenuItem("Credits", showCredit),
        MenuItem("Quit", &menuInfoQuit, fake)};
    Menu menu = Menu("Main Menu", items);
    menuManager.setCurrentMenu(&menu);

    // --- Main loop ---

    auto frameDuration = duration_cast<nanoseconds>(seconds(1)) / max_fps;
    const auto frameDurationTick = duration_cast<steady_clock::duration>(frameDuration);
    auto nextFrameDeadline = steady_clock::now();
    auto fastForwardStatsStarted = steady_clock::now();
    uint64_t fastForwardCoreRuns = 0;
    uint64_t fastForwardCoreTimeUs = 0;
    uint64_t fastForwardAchievementsTimeUs = 0;
    bool previousFastForwardState = false;

    while (isRunning)
    {
        decoration_catalog_update();
#ifndef RR_PLATFORM_SDL
        auto loopStart = steady_clock::now();
#endif
        input_message = false;
        const auto achievementsStarted = steady_clock::now();
        if (pause_requested)
            achievements_idle();
        else
            achievements_frame();
        if (input_ffwd_requested)
            fastForwardAchievementsTimeUs += duration_cast<microseconds>(
                steady_clock::now() - achievementsStarted).count();
        auto nextClock = high_resolution_clock::now();
        bool realPause = pause_requested && input_pause_requested;
        bool showInfo = pause_requested && input_info_requested;

        if (input_info_requested)
        {
            totalFrames = 0;
            core_input_poll();
            if (input_info_requested)
                core_video_refresh(nullptr, 0, 0, 0);
            nextFrameDeadline += frameDurationTick;
            const auto now = steady_clock::now();
            if (nextFrameDeadline > now)
                std::this_thread::sleep_until(nextFrameDeadline);
            else if (now - nextFrameDeadline > frameDurationTick)
                nextFrameDeadline = now;
            continue;
        }
        else if (realPause)
        {
            totalFrames = 0;
            core_input_poll();
        }
        else
        {
            if (showInfo)
                redrawInfo = false;
            else
                redrawInfo = true;
            const auto coreStarted = steady_clock::now();
            g_retro.retro_run();
            if (input_ffwd_requested) {
                ++fastForwardCoreRuns;
                fastForwardCoreTimeUs += duration_cast<microseconds>(
                    steady_clock::now() - coreStarted).count();
            }
        }

        if (input_ffwd_requested != previousFastForwardState) {
            logger.log(Logger::INF,
                       "Fast-forward runtime: %s, ratio=%.2f, normal_pacing=%s",
                       input_ffwd_requested ? "enabled" : "disabled",
                       fastForwardRatio(), runLoopAtDeclaredfps ? "enabled" : "disabled");
            previousFastForwardState = input_ffwd_requested;
            fastForwardStatsStarted = steady_clock::now();
            fastForwardCoreRuns = 0;
            fastForwardCoreTimeUs = 0;
            fastForwardAchievementsTimeUs = 0;
            uint64_t callbacks = 0, presented = 0, dropped = 0, callbackTimeUs = 0;
            fastForwardVideoStats(&callbacks, &presented, &dropped, &callbackTimeUs);
            (void)fastForwardAudioFramesDropped();
        }
        if (input_ffwd_requested &&
            steady_clock::now() - fastForwardStatsStarted >= seconds(1)) {
            uint64_t callbacks = 0, presented = 0, dropped = 0, callbackTimeUs = 0;
            fastForwardVideoStats(&callbacks, &presented, &dropped, &callbackTimeUs);
            const uint64_t audioDropped = fastForwardAudioFramesDropped();
            logger.log(Logger::INF,
                       "Fast-forward health: retro_run=%llu, video_callbacks=%llu, presented=%llu, dropped=%llu, audio_frames_muted=%llu, avg_retro_run_us=%.1f, avg_video_callback_us=%.1f, avg_achievements_us=%.1f, ratio=%.2f",
                       static_cast<unsigned long long>(fastForwardCoreRuns),
                       static_cast<unsigned long long>(callbacks),
                       static_cast<unsigned long long>(presented),
                       static_cast<unsigned long long>(dropped),
                       static_cast<unsigned long long>(audioDropped),
                       fastForwardCoreRuns ? static_cast<double>(fastForwardCoreTimeUs) / fastForwardCoreRuns : 0.0,
                       callbacks ? static_cast<double>(callbackTimeUs) / callbacks : 0.0,
                       fastForwardCoreRuns ? static_cast<double>(fastForwardAchievementsTimeUs) / fastForwardCoreRuns : 0.0,
                       fastForwardRatio());
            fastForwardCoreRuns = 0;
            fastForwardCoreTimeUs = 0;
            fastForwardAchievementsTimeUs = 0;
            fastForwardStatsStarted = steady_clock::now();
        }

        if (input_exit_requested)
        {
            isRunning = false;
        }
        else if (input_reset_requested)
        {
            input_reset_requested = false;
            g_retro.retro_reset();
            achievements_reset();
        }
        else if (input_slot_memory_load_requested && !continueToShowSaveLoadStateImage())
        {
            loadSaveSlotWrapper(A_BUTTON, currentSlot, "Load");
        }
        else if (input_slot_memory_save_requested && !continueToShowSaveLoadStateImage())
        {
            loadSaveSlotWrapper(A_BUTTON, currentSlot, "Save");
        }

        if (!continueToShowSaveLoadStateImage())
        {
            input_slot_memory_load_requested = false;
            input_slot_memory_save_requested = false;
            input_slot_memory_plus_requested = false;
            input_slot_memory_minus_requested = false;
        }

#ifndef RR_PLATFORM_SDL
        auto loopEnd = steady_clock::now();
        auto loopDuration = duration_cast<nanoseconds>(loopEnd - loopStart);
        const nanoseconds frameDurationNs = duration_cast<nanoseconds>(frameDuration);

        static nanoseconds frameDebt = nanoseconds::zero();
        static unsigned skipCooldown = 0;
        if (adaptiveFrameSkip && runLoopAtDeclaredfps && !input_ffwd_requested &&
            !realPause && !showInfo)
        {
            const nanoseconds tolerance = frameDurationNs / 12;
            if (loopDuration > frameDurationNs + tolerance)
                frameDebt += loopDuration - frameDurationNs;
            else if (loopDuration < frameDurationNs)
                frameDebt = std::max(nanoseconds::zero(),
                                     frameDebt - (frameDurationNs - loopDuration));

            if (skipCooldown > 0)
                --skipCooldown;
            const bool overloaded = frameDebt > frameDurationNs * 3;
            skipNextVideoFrame = overloaded && skipCooldown == 0;
            if (skipNextVideoFrame)
            {
                frameDebt = frameDebt > frameDurationNs
                    ? frameDebt - frameDurationNs : nanoseconds::zero();
                skipCooldown = 6;
            }
        }
        else
        {
            frameDebt = nanoseconds::zero();
            skipNextVideoFrame = false;
            skipCooldown = 0;
        }
#endif

        const float fastForwardSpeed = fastForwardRatio();
        if (runLoopAtDeclaredfps && !input_ffwd_requested)
        {
            nextFrameDeadline += frameDurationTick;
            const auto now = steady_clock::now();
            if (nextFrameDeadline > now)
                std::this_thread::sleep_until(nextFrameDeadline);
            else if (now - nextFrameDeadline > frameDurationTick)
                nextFrameDeadline = now;
        }
        else if (input_ffwd_requested && fastForwardSpeed >= 1.0f)
        {
            const auto fastFrameDuration = duration_cast<steady_clock::duration>(
                duration<double>(1.0 / (info.timing.fps * fastForwardSpeed)));
            nextFrameDeadline += fastFrameDuration;
            const auto now = steady_clock::now();
            if (nextFrameDeadline > now)
                std::this_thread::sleep_until(nextFrameDeadline);
            else if (now - nextFrameDeadline > fastFrameDuration)
                nextFrameDeadline = now;
        }
        else
        {
            nextFrameDeadline = steady_clock::now();
        }

        prevClock = nextClock;
        totClock = high_resolution_clock::now();
        totalFrames++;
        elapsed += (totClock - nextClock).count() / 1e9;
#ifdef RR_PLATFORM_SDL
        newFps = std::lround(totalFrames / elapsed);
#else
        newFps = (int)(totalFrames / elapsed);
#endif
        retrorunLoopSkip = newFps;

        if (!startCalAvgFps)
        {
            auto current_time = steady_clock::now();
            auto elapsed_time = duration_cast<std::chrono::seconds>(current_time - start_time).count();
            if (elapsed_time >= 7)
                startCalAvgFps = true;
        }

        if (startCalAvgFps && !(realPause || (showInfo && !redrawInfo)) && newFps > 0 && !input_ffwd_requested)
        {
            countNumFps++;
            countValFps += newFps;
            avgFps = countValFps / countNumFps;
        }

        retrorunLoopCounter++;
        bool drawFps = false;
        if (retrorunLoopCounter >= retrorunLoopSkip)
        {
            drawFps = true;
#ifdef RR_PLATFORM_SDL
            newFps = std::lround(totalFrames / elapsed);
#else
            newFps = (int)(totalFrames / elapsed);
#endif
            retrorunLoopCounter = 0;
        }

        if (adaptiveFps && !input_ffwd_requested)
        {
            if (previous_fps <= newFps)
            {
                max_fps = newFps < info.timing.fps / 2 ? (info.timing.fps / 2) + 10 : info.timing.fps;
                max_fps = newFps < info.timing.fps * 2 / 3 ? (info.timing.fps * 2 / 3) + 5 : info.timing.fps;
            }
            else
            {
                max_fps = info.timing.fps;
            }
            previous_fps = newFps;
        }

        if (drawFps)
        {
            if (!input_ffwd_requested)
                fps = newFps;

            if (opt_show_fps
#ifndef RR_PLATFORM_SDL
                && elapsed >= 1.0
#endif
            )
            {
                logger.log(Logger::DEB, "FPS: %f", fps);
            }
            totalFrames = 0;
            elapsed = 0;
        }
    }

    // --- Cleanup ---

    logger.log(Logger::DEB, "Exiting from render loop...");
    logger.log(Logger::DEB, "Saving sram into file:%s", sramPath);
    SaveSram(sramPath);
    free(sramPath);
    usleep(500000);

    if (auto_save)
    {
        logger.log(Logger::DEB, "Saving sav into file:%s", savePath);
        SaveState(savePath);
        free(savePath);
        sleep(1);
    }

    logger.log(Logger::DEB, "Unloading core and deinit audio and video...");
    network_status_shutdown();
    decoration_catalog_shutdown();
    achievements_shutdown();
    decoration_shutdown();
    fastForwardResetOverride();

#ifdef RR_PLATFORM_SDL
    video_prepare_core_unload();
    if (g_retro.initialized)
    {
        g_retro.retro_unload_game();
        g_retro.retro_deinit();
        g_retro.initialized = false;
    }
    audio_deinit();
    video_deinit();
    if (g_retro.handle)
    {
        dlclose(g_retro.handle);
        g_retro.handle = nullptr;
    }
#else
    video_deinit();
    audio_deinit();

    pthread_t threadId;
    pthread_create(&threadId, NULL, &core_unload, NULL);
    usleep(500000);
    if (exitFlag == 0)
    {
        pthread_join(threadId, NULL);
    }
    else
    {
        pthread_kill(threadId, SIGUSR1);
        pthread_join(threadId, NULL);
        logger.log(Logger::DEB, "Force exiting retrorun.");
        throw std::runtime_error("Force exiting retrorun.\n");
    }
#endif

    return 0;
}
