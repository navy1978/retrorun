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
#include "benchmark.h"

#ifdef RR_PLATFORM_SDL
#include <SDL.h>
#endif

#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cerrno>
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
    {"analog-to-digital", required_argument, NULL, 'A'},
    {"fps", no_argument, NULL, 'f'},
    {"config", required_argument, NULL, 'c'},
    {"benchmark", required_argument, NULL, 1000},
    {"benchmark-warmup", required_argument, NULL, 1001},
    {"benchmark-json", required_argument, NULL, 1002},
    {"benchmark-set", required_argument, NULL, 1003},
    {0, 0, 0, 0}};

static bool parseDuration(const char* text, bool allow_zero, double* result)
{
    if (!text || !*text || !result)
        return false;
    errno = 0;
    char* end = nullptr;
    const double value = std::strtod(text, &end);
    if (errno == ERANGE || end == text || *end != '\0' || !std::isfinite(value))
        return false;
    if (allow_zero ? value < 0.0 : value <= 0.0)
        return false;
    *result = value;
    return true;
}

static bool parseBenchmarkBool(const std::string& text, bool* result)
{
    if (text == "true" || text == "on" || text == "enabled" || text == "1") {
        *result = true;
        return true;
    }
    if (text == "false" || text == "off" || text == "disabled" || text == "0") {
        *result = false;
        return true;
    }
    return false;
}

static bool applyBenchmarkSetting(const std::string& setting, std::string* error)
{
    const size_t separator = setting.find('=');
    if (separator == std::string::npos || separator == 0 || separator + 1 >= setting.size()) {
        if (error) *error = "expected NAME=VALUE";
        return false;
    }
    const std::string name = setting.substr(0, separator);
    const std::string value = setting.substr(separator + 1);
#ifdef RR_HYBRID_AUDIO
    if (name == "audio_backend") {
        if (!rr_audio_backend_select(value.c_str())) {
            if (error) *error = "audio_backend must be auto, go2 or sdl2";
            return false;
        }
        return true;
    }
#endif
    bool enabled = false;
    if (name == "confirm_input_delay") {
        double parsed = 0.0;
        if (!parseDuration(value.c_str(), true, &parsed) || parsed > 60.0) {
            if (error) *error = "confirm_input_delay must be from 0 to 60 seconds";
            return false;
        }
        benchmark_set_confirm_input_delay(parsed);
        return true;
    }
    if (name == "audio_buffer") {
        errno = 0;
        char* end = nullptr;
        const long parsed = std::strtol(value.c_str(), &end, 10);
        if (errno == ERANGE || end == value.c_str() || *end != '\0' ||
            (parsed != -1 && (parsed < 64 || parsed > 8192))) {
            if (error) *error = "audio_buffer must be -1 or an integer from 64 to 8192";
            return false;
        }
        retrorun_audio_buffer = static_cast<int>(parsed);
        new_retrorun_audio_buffer = retrorun_audio_buffer;
        return true;
    }
    if (name == "sdl_audio_stretch_percent" ||
        name == "go2_audio_stretch_percent" ||
        name == "sdl_audio_stretch_low_ms" ||
        name == "go2_audio_stretch_low_ms") {
        errno = 0;
        char* end = nullptr;
        const long parsed = std::strtol(value.c_str(), &end, 10);
        const bool is_percent =
            name.find("stretch_percent") != std::string::npos;
        const long maximum = is_percent ? 10 : 200;
        if (errno == ERANGE || end == value.c_str() || *end != '\0' ||
            parsed < 0 || parsed > maximum) {
            if (error)
                *error = is_percent
                    ? "audio stretch percent must be from 0 to 10"
                    : "audio stretch low watermark must be from 0 to 200 ms";
            return false;
        }
        int* destination = nullptr;
        if (name == "sdl_audio_stretch_percent")
            destination = &retrorun_sdl_audio_stretch_percent;
        else if (name == "go2_audio_stretch_percent")
            destination = &retrorun_go2_audio_stretch_percent;
        else if (name == "sdl_audio_stretch_low_ms")
            destination = &retrorun_sdl_audio_stretch_low_ms;
        else
            destination = &retrorun_go2_audio_stretch_low_ms;
        *destination = static_cast<int>(parsed);
        return true;
    }
    if (name == "fixed_frameskip") {
        errno = 0;
        char* end = nullptr;
        const long parsed = std::strtol(value.c_str(), &end, 10);
        if (errno == ERANGE || end == value.c_str() || *end != '\0' || parsed < 0 || parsed > 5) {
            if (error) *error = "fixed_frameskip must be an integer from 0 to 5";
            return false;
        }
        fixedFrameSkip = static_cast<int>(parsed);
        if (fixedFrameSkip > 0)
            adaptiveFrameSkip = false;
        return true;
    }
    if (!parseBenchmarkBool(value, &enabled)) {
        if (error) *error = "boolean value must be true or false";
        return false;
    }
    if (name == "declared_fps_pacing") runLoopAtDeclaredfps = enabled;
    else if (name == "threaded_audio") forceAudioMultithread = enabled;
    else if (name == "stable_audio_buffer") retrorun_audio_stable_buffer = enabled;
    else if (name == "threaded_video") forceVideoMultithread = enabled;
    else if (name == "direct_scanout")
        drmDirectScanoutMode = enabled ? DRMDirectScanoutMode::Enabled
                                       : DRMDirectScanoutMode::Disabled;
    else if (name == "vsync") {
        rr_video_vsync_set(enabled);
    }
    else if (name == "fps_overlay") input_fps_requested = enabled;
    else if (name == "decorations") conf_map["retrorun_decorations"] = enabled ? "auto" : "off";
    else if (name == "adaptive_frameskip") {
        adaptiveFrameSkip = enabled;
        if (adaptiveFrameSkip)
            fixedFrameSkip = 0;
    }
    else if (name == "confirm_input") benchmark_set_confirm_input(enabled);
    else {
        if (error) *error = "unsupported setting name";
        return false;
    }
    return true;
}

// --- Main ---

int main(int argc, char *argv[])
{
    main_thread_id = pthread_self();
    printf("\n");
    printf("########### RETRORUN %s ###########\n", release.c_str());
    printf("Lightweight cross-platform libretro frontend\n");
    printf("Copyright (C) 2020  OtherCrashOverride\n");
    printf("Copyright (C) 2021-present  navy1978\n");
    printf("\n");

    int c;
    int option_index = 0;

    std::string analogModeOverride;
    BenchmarkOptions benchmarkOptions;
    bool benchmarkOptionSeen = false;
    bool benchmarkModifierSeen = false;
    std::vector<std::string> benchmarkSettings;
    while ((c = getopt_long(argc, argv, "s:d:a:b:v:grtnfc:A:", longopts, &option_index)) != -1)
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
            analogModeOverride = "none";
            break;
        case 'A':
            analogModeOverride = optarg;
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
        case 1000:
            benchmarkOptionSeen = true;
            if (!parseDuration(optarg, false, &benchmarkOptions.duration_seconds))
            {
                std::fprintf(stderr, "Invalid --benchmark duration '%s'.\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 1001:
            benchmarkModifierSeen = true;
            if (!parseDuration(optarg, true, &benchmarkOptions.warmup_seconds))
            {
                std::fprintf(stderr, "Invalid --benchmark-warmup duration '%s'.\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 1002:
            benchmarkModifierSeen = true;
            benchmarkOptions.json_path = optarg;
            break;
        case 1003:
            benchmarkModifierSeen = true;
            benchmarkSettings.emplace_back(optarg);
            break;
        default:
            logger.log(Logger::ERR, "Unknown option. '%s'", longopts[option_index].name);
            exit(EXIT_FAILURE);
        }
    }

    getDeviceName();
    initConfig();

    if (benchmarkModifierSeen && !benchmarkOptionSeen)
    {
        std::fprintf(stderr, "--benchmark-warmup, --benchmark-json and --benchmark-set require --benchmark.\n");
        return EXIT_FAILURE;
    }

    if (benchmarkOptionSeen)
    {
        std::string benchmarkError;
        if (!benchmark_configure(benchmarkOptions, &benchmarkError))
        {
            logger.log(Logger::ERR, "%s", benchmarkError.c_str());
            return EXIT_FAILURE;
        }
        // Benchmark runs are deliberately non-persistent. State loading may
        // still be requested to make repeated runs start from the same point.
        auto_save = false;
        for (const std::string& setting : benchmarkSettings)
        {
            std::string settingError;
            if (!applyBenchmarkSetting(setting, &settingError))
            {
                logger.log(Logger::ERR, "Invalid --benchmark-set '%s': %s.",
                           setting.c_str(), settingError.c_str());
                return EXIT_FAILURE;
            }
            logger.log(Logger::INF, "Benchmark override (not persisted): %s", setting.c_str());
        }
        logger.log(Logger::INF,
                   "Benchmark requested: duration=%.3f seconds, warmup=%.3f seconds, saves=disabled",
                   benchmarkOptions.duration_seconds, benchmarkOptions.warmup_seconds);
    }

    if (!analogModeOverride.empty())
    {
        if (!setAnalogToDigitalMode(analogModeOverride))
        {
            logger.log(Logger::ERR,
                       "Invalid --analog-to-digital mode '%s' (expected none, left, right, left_forced or right_forced).",
                       analogModeOverride.c_str());
            exit(EXIT_FAILURE);
        }
        logger.log(Logger::INF, "Command-line analog-to-digital mode: %s.",
                   analogToDigitalModeName(analogToDigital));
    }

    if (!gpio_joypad)
    {
        if (isRG351MP() || isRG552())
            gpio_joypad = true;
    }

    int remaining_args = argc - optind;
    int remaining_index = optind;
    logger.log(Logger::DEB, "remaining_args=%d", remaining_args);

    if (remaining_args != 2)
    {
        logger.log(Logger::ERR,
                   "Usage: %s [--benchmark seconds] [--benchmark-warmup seconds] [--benchmark-json file] "
                   "[--benchmark-set NAME=VALUE] core rom",
                   argv[0]);
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
            input_slot_memory_load_requested = true;
            lastLoadSaveStateRequestTime = static_cast<double>(time(NULL));
            StartLoadStateAsync(savePath, 0, true);
        }
    }

    logger.log(Logger::DEB, "Loading sram - File '%s'", sramPath);
    LoadSram(sramPath);
    logger.log(Logger::DEB, "Entering render loop.");

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

    double max_fps = info.timing.fps;
    double previous_fps = 0;
    originalFps = info.timing.fps;
    if (max_fps < 1) max_fps = 60;
    if (originalFps < 1) originalFps = 60;

    if (benchmark_requested())
    {
        BenchmarkMetadata metadata;
        metadata.release = release;
        metadata.device = getDeviceName();
        metadata.backend = rr_platform_backend_name();
#ifdef RR_HYBRID_AUDIO
        metadata.audio_backend = rr_audio_backend_name();
#else
        metadata.audio_backend = metadata.backend;
#endif
        metadata.renderer = rr_platform_renderer_name();
        metadata.core_name = coreName;
        metadata.core_version = coreVersion;
        metadata.declared_fps = info.timing.fps;
        metadata.sample_rate = info.timing.sample_rate;
        metadata.declared_fps_pacing = runLoopAtDeclaredfps;
        metadata.threaded_audio = forceAudioMultithread;
        metadata.stable_audio_buffer = retrorun_audio_stable_buffer;
        metadata.audio_buffer = retrorun_audio_buffer;
        metadata.sdl_audio_stretch_percent =
            retrorun_sdl_audio_stretch_percent;
        metadata.sdl_audio_stretch_low_ms =
            retrorun_sdl_audio_stretch_low_ms;
        metadata.go2_audio_stretch_percent =
            retrorun_go2_audio_stretch_percent;
        metadata.go2_audio_stretch_low_ms =
            retrorun_go2_audio_stretch_low_ms;
        metadata.threaded_video = forceVideoMultithread;
        metadata.direct_scanout = drmDirectScanoutMode == DRMDirectScanoutMode::Enabled;
        metadata.overlays = decoration_surface() != nullptr || opt_show_fps ||
                            input_fps_requested;
        metadata.vsync_requested = rr_video_vsync_get();
        metadata.vsync_applied = rr_video_vsync_applied();
        metadata.fixed_frameskip = fixedFrameSkip;
        metadata.adaptive_frameskip = adaptiveFrameSkip;
        metadata.confirm_input = benchmark_confirm_input_enabled();
        metadata.confirm_input_delay_seconds = benchmark_confirm_input_delay();
        benchmark_set_metadata(metadata);
    }

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
    auto fpsWindowStarted = start_time;
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
        MenuItem("Analog to digital", getAnalogToDigitalSetting,
                 setAnalogToDigitalSetting, "analog-to-digital"),
        MenuItem("Swap triggers", getSwapTriggers, setSwapTriggers, "bool"),
        MenuItem("Swap analog sticks", getSwapSticks, setSwapSticks, "bool"),
        MenuItem("Alternative hotkeys", getAlternativeInputSetting,
                 setAlternativeInputSetting, "bool"),
        MenuItem("Mouse speed", getMouseSpeedSetting, setMouseSpeedSetting, ""),
        MenuItem("Rumble test", []() { return 0; }, [](int val) { testRumble(val); }, "test-rumble"),
        MenuItem("Rumble disabled", getRumbleDisabled, setRumbleDisabled, "bool")};
    Menu menuControl = Menu("Control", itemsControl);

    std::vector<MenuItem> itemsAudio = {
        MenuItem("Audio buffer", getAudioBuffer, setAudioBuffer, "audio-buffer"),
        MenuItem("Stable buffer (restart)", getStableAudioSetting,
                 setStableAudioSetting, "bool"),
        MenuItem("Threaded audio (restart)", getThreadedAudioSetting,
                 setThreadedAudioSetting, "bool"),
        MenuItem("Audio disabled", getAudioDisabled, setAudioDisabled, "bool")};
    Menu menuAudio = Menu("Audio", itemsAudio);

    std::vector<MenuItem> itemsSaves = {
        MenuItem("Auto save", getAutoSaveSetting, setAutoSaveSetting, "bool"),
        MenuItem("Auto load (next start)", getAutoLoadSetting, setAutoLoadSetting, "bool")};
    Menu menuSaves = Menu("Saves", itemsSaves);

    std::vector<MenuItem> itemsAchievements = {
        MenuItem(achievements_status_label, [](int) {}),
        MenuItem(achievements_username_label,
                 [](int button) { if (button == A_BUTTON) achievements_edit_username(arg_rom); }),
        MenuItem("Set password",
                 [](int button) { if (button == A_BUTTON) achievements_edit_password(arg_rom); }),
        MenuItem("Enabled",
                 []() { return achievements_enabled() ? 1 : 0; },
                 [](int button) {
                     if (button == LEFT || button == RIGHT)
                         achievements_set_enabled(!achievements_enabled(), arg_rom);
                 },
                 "bool"),
        MenuItem("Unofficial (restart)", getAchievementsUnofficialSetting,
                 setAchievementsUnofficialSetting, "bool"),
        MenuItem("Encore (restart)", getAchievementsEncoreSetting,
                 setAchievementsEncoreSetting, "bool")};
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
        MenuItem("FPS counter", getFPSCounterSetting, setFPSCounterSetting, "bool"),
        MenuItem("Lock FPS", getLockDeclaredFPS, setLockDeclaredFPS, "bool"),
        MenuItem("UI profile", getUIProfileSetting, setUIProfileSetting, "ui-profile"),
        MenuItem("Decorations", &menuDecorations, fake),
#ifdef RR_PLATFORM_SDL
        MenuItem("Renderer (restart)", getSDLVideoRenderer, setSDLVideoRenderer, "video-renderer"),
        MenuItem("VSync", getSDLVsync, setSDLVsync, "bool"),
#endif
        MenuItem("Video filter", getVideoFilter, setVideoFilter, "video-filter"),
        MenuItem("Shader (restart)", getVideoShader, setVideoShader, "video-shader"),
        MenuItem("Tate mode", getTateMode, setTateMode, "rotation"),
        MenuItem("Loading screen (restart)", getLoadingScreenSetting,
                 setLoadingScreenSetting, "bool")};

    std::vector<MenuItem> itemsPerformance = {
        MenuItem("Adaptive frameskip", getAdaptiveFrameskipSetting,
                 setAdaptiveFrameskipSetting, "bool"),
        MenuItem("Fixed frameskip", getFixedFrameskipSetting,
                 setFixedFrameskipSetting, "")};
#ifndef RR_PLATFORM_SDL
    itemsPerformance.emplace_back("DRM direct scanout", getDRMDirectScanoutSetting,
                                  setDRMDirectScanoutSetting, "drm-direct-scanout");
    if (supportsVideoMultithread())
        itemsPerformance.emplace_back("Force threaded video (restart)", getThreadedVideoSetting,
                                      setThreadedVideoSetting, "bool");
#endif
    Menu menuPerformance = Menu("Performance", itemsPerformance);

    std::vector<MenuItem> itemsDiagnostics = {
        MenuItem("RetroRun log (restart)", getRetroRunLogLevelSetting,
                 setRetroRunLogLevelSetting, "log-level"),
        MenuItem("Core log (restart)", getCoreLogLevelSetting,
                 setCoreLogLevelSetting, "log-level"),
        MenuItem("Log to file (restart)", getLogToFileSetting,
                 setLogToFileSetting, "bool"),
        MenuItem("Input key log", getKeyLogSetting, setKeyLogSetting, "bool")};
    Menu menuDiagnostics = Menu("Diagnostics", itemsDiagnostics);

    MenuItem menuItem_restart_core = MenuItem("Are you sure?", [](int arg) { restartCore(arg); });
    menuItem_restart_core.setQuestionItem();
    std::vector<MenuItem> restartCore_sure = {menuItem_restart_core};
    Menu menuResetCore = Menu("Reset Core", restartCore_sure);

    Menu menuVideo = Menu("Video", itemsVideo);

    std::vector<MenuItem> itemsSettings = {
        MenuItem("System", &menuSystem, fake),
        MenuItem("Saves", &menuSaves, fake),
        MenuItem("Control", &menuControl, fake),
        MenuItem("Video", &menuVideo, fake),
        MenuItem("Audio", &menuAudio, fake),
        MenuItem("Performance", &menuPerformance, fake),
        MenuItem("RetroAchievements", &menuAchievements, fake),
        MenuItem("Diagnostics", &menuDiagnostics, fake),
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
#ifndef RR_PLATFORM_SDL
    steady_clock::time_point drmDiagnosticDeadline{};
    bool drmDiagnosticSavedFps = false;
    bool drmDiagnosticSavedFastForward = false;
    MenuItem drmDiagnosticQuestion = MenuItem("Start DRM test?", [&](int button) {
        if (button != A_BUTTON)
            return;
        rr_display_diagnostics_reset(display);
        drmDirectScanoutDiagnosticCompleted = false;
        drmDirectScanoutDiagnosticActive = true;
        drmDiagnosticSavedFps = input_fps_requested;
        drmDiagnosticSavedFastForward = input_ffwd_requested;
        input_fps_requested = false;
        input_ffwd_requested = false;
        input_info_requested = false;
        input_credits_requested = false;
        pause_requested = false;
        drmDiagnosticDeadline = steady_clock::now() + seconds(3);
        logger.log(Logger::INF,
                   "DRM diagnostic started: direct scanout forced for 3 seconds; input disabled.");
    });
    drmDiagnosticQuestion.setQuestionItem();
    std::vector<MenuItem> drmDiagnosticQuestionItems = {drmDiagnosticQuestion};
    Menu menuDRMDiagnosticQuestion = Menu(
        "Game runs for 3 seconds; input disabled", drmDiagnosticQuestionItems);
    std::vector<MenuItem> drm = {
        MenuItem(SHOW_DRM, &menuDRMDiagnosticQuestion, fake)};
    Menu menuInfoDRM = Menu("DRM diagnostics", drm);
#endif
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
#ifndef RR_PLATFORM_SDL
        MenuItem("DRM diagnostics", &menuInfoDRM, fake),
#endif
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
    auto frameDurationTick = duration_cast<steady_clock::duration>(frameDuration);
    auto nextFrameDeadline = steady_clock::now();
    auto fastForwardStatsStarted = steady_clock::now();
    uint64_t fastForwardCoreRuns = 0;
    uint64_t fastForwardCoreTimeUs = 0;
    uint64_t fastForwardAchievementsTimeUs = 0;
    bool previousFastForwardState = false;
    bool audioTransitionPaused = false;

    if (benchmark_requested())
        benchmark_begin_warmup();

    while (isRunning)
    {
        if (benchmark_requested())
        {
            if (benchmark_deadline_reached())
            {
                isRunning = false;
                break;
            }
            // The benchmark owns the run window: frontend hotkeys must not
            // pause, reset, save, or open pages midway through a sample.
            input_info_requested = false;
            input_credits_requested = false;
            input_pause_requested = false;
            input_ffwd_requested = false;
            input_reset_requested = false;
            input_slot_memory_load_requested = false;
            input_slot_memory_save_requested = false;
            pause_requested = false;
        }
        const bool measureBenchmarkFrame = benchmark_collecting();
        if (measureBenchmarkFrame)
            benchmark_frame_begin();
#ifndef RR_PLATFORM_SDL
        if (drmDirectScanoutDiagnosticActive &&
            steady_clock::now() >= drmDiagnosticDeadline)
        {
            drmDirectScanoutDiagnosticActive = false;
            drmDirectScanoutDiagnosticCompleted = true;
            input_fps_requested = drmDiagnosticSavedFps;
            input_ffwd_requested = drmDiagnosticSavedFastForward;
            input_info_requested = true;
            input_credits_requested = false;
            pause_requested = true;
            menuManager.setCurrentMenu(&menuInfoDRM);
            rr_display_diagnostics_t result = {};
            rr_display_diagnostics_get(display, &result);
            logger.log(Logger::INF,
                       "DRM diagnostic result: driver='%s', mode=%dx%d@%d, plane=%u, format=0x%08x, frames=%llu, vblank_us=%u/%u/%u, vblank_errors=%u, plane_errno=%d",
                       result.driver, result.mode_width, result.mode_height,
                       result.refresh_hz, static_cast<unsigned>(result.plane_id),
                       static_cast<unsigned>(result.plane_format),
                       static_cast<unsigned long long>(result.direct_frames),
                       static_cast<unsigned>(result.vblank_average_us),
                       static_cast<unsigned>(result.vblank_last_us),
                       static_cast<unsigned>(result.vblank_max_us),
                       static_cast<unsigned>(result.vblank_failures),
                       result.direct_errno);
            logger.log(Logger::INF,
                       "DRM diagnostic completed; reopening the results page.");
        }
#endif
        decoration_catalog_update();
        // Input is polled by the core during retro_run(), so fast-forward may
        // toggle in the middle of this iteration. Use the state captured at
        // frame start for optional profiling and begin measuring on the next
        // complete fast-forward frame.
        const bool profileFastForwardFrame = input_ffwd_requested;
#ifndef RR_PLATFORM_SDL
        // Timing the loop is useful only to adaptive frameskip. Avoid an
        // otherwise unnecessary clock read on every frame when it is off.
        const bool measureAdaptiveLoop = adaptiveFrameSkip &&
            runLoopAtDeclaredfps && !input_ffwd_requested;
        const auto loopStart = measureAdaptiveLoop
            ? steady_clock::now() : steady_clock::time_point{};
#endif
        input_message = false;
        const auto achievementsStarted = profileFastForwardFrame
            ? steady_clock::now() : steady_clock::time_point{};
        double factor = static_cast<double>(0b110001) / 0b110010;
        if (pause_requested)
            achievements_idle();
        else
            achievements_frame();
        if (profileFastForwardFrame)
            fastForwardAchievementsTimeUs += duration_cast<microseconds>(
                steady_clock::now() - achievementsStarted).count();
        bool realPause = pause_requested && input_pause_requested;
        bool showInfo = pause_requested && input_info_requested;
        const bool shouldPauseAudio = pause_requested;
        if (shouldPauseAudio != audioTransitionPaused)
        {
            if (shouldPauseAudio)
                audio_pause();
            else
                audio_resume();
            audioTransitionPaused = shouldPauseAudio;
        }

        if (input_info_requested)
        {
            totalFrames = 0;
            fpsWindowStarted = steady_clock::now();
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
            fpsWindowStarted = steady_clock::now();
            core_input_poll();
        }
        else
        {
            if (showInfo)
                redrawInfo = false;
            else
                redrawInfo = true;
            const auto coreStarted = profileFastForwardFrame
                ? steady_clock::now() : steady_clock::time_point{};
            if (measureBenchmarkFrame)
                benchmark_core_begin();
            g_retro.retro_run();
            if (measureBenchmarkFrame)
                benchmark_core_end();
            PumpLoadStateAsync();

            struct retro_system_av_info pendingInfo = {};
            if (core_take_pending_av_info(&pendingInfo))
            {
                const bool timingValid = std::isfinite(pendingInfo.timing.fps) &&
                    pendingInfo.timing.fps > 0.0 &&
                    std::isfinite(pendingInfo.timing.sample_rate) &&
                    pendingInfo.timing.sample_rate > 0.0;
                const bool geometryApplied = video_reconfigure_geometry(&pendingInfo.geometry);
                const bool audioApplied = timingValid &&
                    audio_reconfigure(static_cast<int>(std::lround(pendingInfo.timing.sample_rate)),
                                      pendingInfo.timing.fps);
                if (!timingValid || !geometryApplied || !audioApplied)
                {
                    logger.log(Logger::ERR, "Unable to apply runtime system AV information safely");
                    benchmark_abort("runtime system AV update failed");
                    isRunning = false;
                }
                else
                {
                    info = pendingInfo;
                    max_fps = pendingInfo.timing.fps;
                    originalFps = pendingInfo.timing.fps;
                    frameDuration = duration_cast<nanoseconds>(seconds(1)) / max_fps;
                    frameDurationTick = duration_cast<steady_clock::duration>(frameDuration);
                    nextFrameDeadline = steady_clock::now();
                    benchmark_update_av(pendingInfo.timing.fps,
                                        pendingInfo.timing.sample_rate);
                    logger.log(Logger::INF,
                               "Runtime AV timing applied: fps=%.6f sample_rate=%.3f",
                               pendingInfo.timing.fps, pendingInfo.timing.sample_rate);
                }
            }
            if (profileFastForwardFrame) {
                ++fastForwardCoreRuns;
                fastForwardCoreTimeUs += duration_cast<microseconds>(
                    steady_clock::now() - coreStarted).count();
            }
        }
        PumpLoadStateAsync();

        if (input_ffwd_requested != previousFastForwardState) {
            if (input_ffwd_requested)
                audio_discard_pending();
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
            core_reset_synchronized();
            achievements_reset();
        }
        else if (input_slot_memory_load_requested && !LoadStateAsyncBusy() &&
                 !continueToShowSaveLoadStateImage())
        {
            loadSaveSlotWrapper(A_BUTTON, currentSlot, "Load");
        }
        else if (input_slot_memory_save_requested && !continueToShowSaveLoadStateImage())
        {
            loadSaveSlotWrapper(A_BUTTON, currentSlot, "Save");
        }

        if (!LoadStateAsyncBusy() && !continueToShowSaveLoadStateImage())
        {
            input_slot_memory_load_requested = false;
            input_slot_memory_save_requested = false;
            input_slot_memory_plus_requested = false;
            input_slot_memory_minus_requested = false;
        }

#ifndef RR_PLATFORM_SDL
        const nanoseconds frameDurationNs = duration_cast<nanoseconds>(frameDuration);

        static nanoseconds frameDebt = nanoseconds::zero();
        static unsigned skipCooldown = 0;
        if (measureAdaptiveLoop && !input_ffwd_requested &&
            !realPause && !showInfo)
        {
            const auto loopDuration = duration_cast<nanoseconds>(
                steady_clock::now() - loopStart);
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

        if (measureBenchmarkFrame)
            benchmark_frame_end(nextFrameDeadline + frameDurationTick,
                                runLoopAtDeclaredfps && !input_ffwd_requested);

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

        if (!realPause)
            ++totalFrames;
        ++retrorunLoopCounter;
        // Checking a one-second FPS window does not require querying the
        // monotonic clock on every frame. Four checks per second at 60 fps are
        // enough while also remaining responsive on slower cores.
        if (retrorunLoopCounter >= 15)
        {
            retrorunLoopCounter = 0;
            const auto fpsNow = steady_clock::now();
            double fpsElapsed = duration<double>(
                fpsNow - fpsWindowStarted).count();
            if (fpsElapsed >= 1.0)
            {
                const double measuredFps = std::ceil(totalFrames / (fpsElapsed*factor));
                const double declaredFpsCeiling = std::ceil(
                    static_cast<double>(originalFps));
                const double displayedFps = runLoopAtDeclaredfps &&
                    !input_ffwd_requested
                        ? std::min(measuredFps, declaredFpsCeiling)
                        : measuredFps;
                newFps = static_cast<float>(displayedFps);
                retrorunLoopSkip = std::max(1, static_cast<int>(newFps));

                if (!startCalAvgFps && fpsNow - start_time >= seconds(7))
                    startCalAvgFps = true;

                // Sample the average once per measurement window. The previous
                // code accumulated the same partial-window estimate every frame,
                // doing needless floating-point divisions and biasing the result.
                if (startCalAvgFps && !(realPause || (showInfo && !redrawInfo)) &&
                    newFps > 0 && !input_ffwd_requested)
                {
                    ++countNumFps;
                    countValFps += static_cast<unsigned long long>(newFps);
                    avgFps = static_cast<float>(countValFps) / countNumFps;
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

                if (!input_ffwd_requested)
                    fps = newFps;

                if (opt_show_fps)
                    logger.log(Logger::DEB, "FPS: %f", fps);

                totalFrames = 0;
                fpsWindowStarted = fpsNow;
            }
        }
    }

    // --- Cleanup ---

    logger.log(Logger::DEB, "Exiting from render loop...");
    if (!benchmark_requested())
    {
        logger.log(Logger::DEB, "Saving sram into file:%s", sramPath);
        SaveSram(sramPath);
    }
    else
    {
        logger.log(Logger::INF, "Benchmark mode: SRAM and savestate writes skipped.");
    }
    free(sramPath);
    if (!benchmark_requested())
        usleep(500000);

    if (auto_save && !benchmark_requested())
    {
        logger.log(Logger::DEB, "Saving sav into file:%s", savePath);
        SaveState(savePath);
        sleep(1);
    }
    free(savePath);

    logger.log(Logger::DEB, "Unloading core and deinit audio and video...");
    ShutdownLoadStateAsync();
    logger.log(Logger::DEB, "Shutdown: save-state loader stopped");
    network_status_shutdown();
    logger.log(Logger::DEB, "Shutdown: network status stopped");
    decoration_catalog_shutdown();
    logger.log(Logger::DEB, "Shutdown: decoration catalog stopped");
    achievements_shutdown();
    logger.log(Logger::DEB, "Shutdown: achievements stopped");
    decoration_shutdown();
    logger.log(Logger::DEB, "Shutdown: decorations stopped");
    fastForwardResetOverride();

    core_disable_callbacks();
    logger.log(Logger::DEB, "Shutdown: core callbacks disabled");
    audio_deinit();
    logger.log(Logger::DEB, "Shutdown: audio stopped");
    video_prepare_core_unload();
    logger.log(Logger::DEB, "Shutdown: video workers stopped and hardware context released");
    core_unload_game();
    logger.log(Logger::DEB, "Shutdown: game unloaded");
    core_deinit();
    logger.log(Logger::DEB, "Shutdown: core unloaded and deinitialized");
    video_deinit();
    logger.log(Logger::DEB, "Shutdown: video resources destroyed");
    input_deinit();
    logger.log(Logger::DEB, "Shutdown: input resources destroyed");

    // The legacy Flycast 2021 shared object hangs from its ELF finalizers when
    // dlclose() is called, even after retro_deinit() has completed. Every
    // frontend resource and persistent save has already been released at this
    // point, so let process exit unload this one faulty core. This is the safe
    // equivalent of the historical GO2 SIGUSR1 shutdown workaround and is
    // required for interactive sessions as well as benchmarks.
    if (isFlycast2021())
    {
        const bool benchmarkOk = benchmark_finish_and_report();
        logger.log(Logger::WARN,
                   "Legacy Flycast shutdown complete; exiting without dlclose to avoid hanging ELF finalizers");
        std::fflush(nullptr);
        std::_Exit(benchmarkOk ? 0 : EXIT_FAILURE);
    }
    core_close();
    logger.log(Logger::DEB, "Shutdown: core library closed");

    const bool benchmarkOk = benchmark_finish_and_report();
    return benchmarkOk ? 0 : EXIT_FAILURE;
}
