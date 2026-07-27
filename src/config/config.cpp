/*
retrorun - libretro frontend for Anbernic Devices
Copyright (C) 2020  OtherCrashOverride
Copyright (C) 2021-present  navy1978
*/

#include "config.h"
#include "config/config_file.h"
#include "globals.h"
#include "video-helper.h"
#include "input.h"
#include "rumble.h"
#include "platform.h"
#include "js2xbox/events.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <ctime>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <unordered_set>
#include <vector>

// Whitespace characters for trimming
static const char *ws = " \t\n\r\f\v";

// --- Extern variable definitions ---

std::string activeConfigFile;

#ifdef RR_PLATFORM_SDL
SDLVideoRenderer sdlVideoRenderer = SDLVideoRenderer::Auto;
bool sdlVsync = false;
const char *opt_setting_file = "./retrorun.cfg";
#else
const char *opt_setting_file = "/storage/.config/distribution/configs/retrorun.cfg";
#endif
bool opt_setting_file_explicit = false;

bool opt_show_fps = false;
bool auto_save = false;
bool auto_load = false;

rr_video_filter_t videoFilter = RR_VIDEO_FILTER_DEFAULT;
rr_video_shader_t videoShader = RR_VIDEO_SHADER_OFF;

static std::map<std::string, std::size_t> configSourceLines;

static const std::unordered_set<std::string> &knownRetroRunSettings()
{
    static const std::unordered_set<std::string> settings = {
        "retrorun_achievements_enabled", "retrorun_achievements_encore",
        "retrorun_achievements_password", "retrorun_achievements_token",
        "retrorun_achievements_unofficial", "retrorun_achievements_username",
        "retrorun_adaptive_frameskip", "retrorun_alternative_input_mode",
        "retrorun_analog_to_digital", "retrorun_aspect_ratio",
        "retrorun_audio_backend", "retrorun_audio_buffer",
        "retrorun_audio_stable_buffer", "retrorun_auto_load",
        "retrorun_auto_save", "retrorun_core_log_level",
        "retrorun_decoration_pack", "retrorun_decoration_source",
        "retrorun_decorations", "retrorun_decorations_path",
        "retrorun_device_name", "retrorun_disable_rumble",
        "retrorun_drm_direct_scanout", "retrorun_enable_key_log",
        "retrorun_egl_depth_bits", "retrorun_egl_stencil_bits",
        "retrorun_extra_evdev_name", "retrorun_extra_osh_name",
        "retrorun_extra_retrogame_name", "retrorun_force_audio_multithread",
        "retrorun_flycast_catalog_update",
        "retrorun_flycast_game_profile",
        "retrorun_force_left_analog_stick", "retrorun_force_video_multithread",
        "retrorun_fps_counter", "retrorun_frameskip",
        "retrorun_go2_audio_stretch_low_ms",
        "retrorun_go2_audio_stretch_percent", "retrorun_log_level",
        "retrorun_log_to_file", "retrorun_loop_declared_fps",
        "retrorun_mouse_speed_factor", "retrorun_pixel_perfect",
        "retrorun_rumble_event", "retrorun_rumble_pwm_file",
        "retrorun_rumble_type", "retrorun_screenshot_folder",
        "retrorun_sdl_audio_stretch_low_ms",
        "retrorun_sdl_audio_stretch_percent", "retrorun_show_loading_screen",
        "retrorun_swap_l1r1_with_l2r2", "retrorun_swap_sticks",
        "retrorun_tate_mode", "retrorun_ui_profile",
        "retrorun_video_filter", "retrorun_video_renderer",
        "retrorun_video_shader", "retrorun_vsync"
    };
    return settings;
}

void applyTransientConfigOverrides(
    const std::map<std::string, std::string> &overrides)
{
    for (const auto &[setting, value] : overrides)
    {
        conf_map[setting] = value;
        logger.log(Logger::DEB, "Transient profile override: %s=%s",
                   setting.c_str(), value.c_str());
    }

    if (overrides.find("retrorun_loop_declared_fps") != overrides.end())
        runLoopAtDeclaredfps = configValueIsTrue(
            "retrorun_loop_declared_fps", runLoopAtDeclaredfps);
    if (overrides.find("retrorun_audio_stable_buffer") != overrides.end())
        retrorun_audio_stable_buffer = configValueIsTrue(
            "retrorun_audio_stable_buffer", retrorun_audio_stable_buffer);
    if (overrides.find("retrorun_force_audio_multithread") != overrides.end())
        forceAudioMultithread = configValueIsTrue(
            "retrorun_force_audio_multithread", forceAudioMultithread);
    if (overrides.find("retrorun_force_video_multithread") != overrides.end())
        forceVideoMultithread = configValueIsTrue(
            "retrorun_force_video_multithread", forceVideoMultithread);
    if (overrides.find("retrorun_drm_direct_scanout") != overrides.end())
        drmDirectScanoutMode = configValueIsTrue(
            "retrorun_drm_direct_scanout", false)
                ? DRMDirectScanoutMode::Enabled
                : DRMDirectScanoutMode::Disabled;
    if (overrides.find("retrorun_adaptive_frameskip") != overrides.end())
        adaptiveFrameSkip = configValueIsTrue(
            "retrorun_adaptive_frameskip", adaptiveFrameSkip);
    if (overrides.find("retrorun_frameskip") != overrides.end())
        fixedFrameSkip = configValueInteger(
            "retrorun_frameskip", fixedFrameSkip, 0, 5);
    if (fixedFrameSkip > 0)
        adaptiveFrameSkip = false;

    if (overrides.find("retrorun_go2_audio_stretch_percent") !=
        overrides.end())
        retrorun_go2_audio_stretch_percent = configValueInteger(
            "retrorun_go2_audio_stretch_percent",
            retrorun_go2_audio_stretch_percent, 0, 10);
    if (overrides.find("retrorun_go2_audio_stretch_low_ms") !=
        overrides.end())
        retrorun_go2_audio_stretch_low_ms = configValueInteger(
            "retrorun_go2_audio_stretch_low_ms",
            retrorun_go2_audio_stretch_low_ms, 0, 200);
    if (overrides.find("retrorun_sdl_audio_stretch_percent") !=
        overrides.end())
        retrorun_sdl_audio_stretch_percent = configValueInteger(
            "retrorun_sdl_audio_stretch_percent",
            retrorun_sdl_audio_stretch_percent, 0, 10);
    if (overrides.find("retrorun_sdl_audio_stretch_low_ms") !=
        overrides.end())
        retrorun_sdl_audio_stretch_low_ms = configValueInteger(
            "retrorun_sdl_audio_stretch_low_ms",
            retrorun_sdl_audio_stretch_low_ms, 0, 200);
    if (overrides.find("retrorun_vsync") != overrides.end())
        rr_video_vsync_set(configValueIsTrue("retrorun_vsync", false));
}

static bool isButtonMappingSetting(const std::string &setting)
{
    constexpr const char *prefix = "retrorun_mapping_button_";
    return setting.compare(0, std::strlen(prefix), prefix) == 0;
}

static std::size_t editDistance(const std::string &left,
                                const std::string &right)
{
    std::vector<std::size_t> previous(right.size() + 1);
    std::vector<std::size_t> current(right.size() + 1);
    for (std::size_t index = 0; index <= right.size(); ++index)
        previous[index] = index;

    for (std::size_t row = 1; row <= left.size(); ++row)
    {
        current[0] = row;
        for (std::size_t column = 1; column <= right.size(); ++column)
        {
            const std::size_t substitution =
                previous[column - 1] +
                (left[row - 1] == right[column - 1] ? 0 : 1);
            current[column] = std::min({
                previous[column] + 1,
                current[column - 1] + 1,
                substitution
            });
        }
        previous.swap(current);
    }
    return previous.back();
}

static void warnAboutUnknownRetroRunSettings()
{
    const auto &known = knownRetroRunSettings();
    for (const auto &[setting, value] : conf_map)
    {
        (void)value;
        if (known.find(setting) != known.end() ||
            isButtonMappingSetting(setting))
            continue;

        const std::string *suggestion = nullptr;
        std::size_t bestDistance = 3;
        for (const std::string &candidate : known)
        {
            const std::size_t distance = editDistance(setting, candidate);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                suggestion = &candidate;
            }
        }
        if (setting.compare(0, 9, "retrorun_") == 0 || suggestion)
        {
            const auto line = configSourceLines.find(setting);
            const std::size_t lineNumber =
                line == configSourceLines.end() ? 0 : line->second;
            if (suggestion)
                logger.log(Logger::WARN,
                           "Unknown RetroRun setting '%s' at line %zu; did you mean '%s'?",
                           setting.c_str(), lineNumber, suggestion->c_str());
            else
                logger.log(Logger::WARN,
                           "Unknown RetroRun setting '%s' at line %zu.",
                           setting.c_str(), lineNumber);
        }
    }
}

// --- String utilities ---

std::string &rtrim(std::string &s)
{
    s.erase(s.find_last_not_of(ws) + 1);
    return s;
}

std::string &ltrim(std::string &s)
{
    s.erase(0, s.find_first_not_of(ws));
    return s;
}

std::string &trim(std::string &s)
{
    return ltrim(rtrim(s));
}

// --- Config file parsing ---

void initMapConfig(std::string pathConfFile)
{
    rr::config::Document document;
    std::string error;
    if (!rr::config::load(pathConfFile, document, error))
    {
        conf_map.clear();
        configSourceLines.clear();
        logger.log(Logger::ERR, "Unable to read configuration file '%s': %s",
                   pathConfFile.c_str(), error.c_str());
        return;
    }

    conf_map = std::move(document.values);
    configSourceLines = std::move(document.source_lines);
    for (const rr::config::Diagnostic &diagnostic : document.diagnostics)
    {
        const Logger::LogLevel level =
            diagnostic.level == rr::config::DiagnosticLevel::Error
                ? Logger::ERR : Logger::WARN;
        logger.log(level, "Configuration '%s', line %zu: %s",
                   pathConfFile.c_str(), diagnostic.line,
                   diagnostic.message.c_str());
    }
    logger.log(Logger::DEB,
               "Configuration loaded: %zu setting(s), %zu diagnostic(s).",
               conf_map.size(), document.diagnostics.size());
    warnAboutUnknownRetroRunSettings();
}

// --- Config helpers ---

float getAspectRatio(const std::string aspect)
{
    if (aspect == "2:1")
        return 2.0f;
    else if (aspect == "4:3")
        return 1.333333f;
    else if (aspect == "5:4")
        return 1.25f;
    else if (aspect == "16:9")
        return 1.777777f;
    else if (aspect == "16:10")
        return 1.6f;
    else if (aspect == "1:1")
        return 1.0f;
    else if (aspect == "3:2")
        return 1.5f;
    else if (aspect == "auto")
        return 0.0f;
    else
        return 0.0f;
}

TateState getTateMode(const std::string tate)
{
    if (tate == "enabled")
        return ENABLED;
    else if (tate == "disabled")
        return DISABLED;
    else if (tate == "reversed" || tate == "reverted")
        return REVERSED;
    else if (tate == "auto")
        return AUTO;
    else
        return DISABLED;
}

Logger::LogLevel getLogLevel(const std::string level)
{
    if (level == "INFO")
        return Logger::INF;
    else if (level == "WARNING")
        return Logger::WARN;
    else if (level == "ERROR")
        return Logger::ERR;
    else if (level == "DEBUG")
        return Logger::DEB;
    else
        return Logger::INF;
}

bool fileExists(const char *path)
{
    return access(path, F_OK) != -1;
}

static std::string getAbsolutePath(const std::string &path)
{
    std::error_code error;
    std::filesystem::path absolute = std::filesystem::absolute(path, error);
    if (error)
        return path;

    std::filesystem::path canonical = std::filesystem::weakly_canonical(absolute, error);
    if (!error)
        return canonical.string();

    return absolute.lexically_normal().string();
}

std::string getLastModifiedTime(const char *path)
{
    struct stat fileInfo;
    if (stat(path, &fileInfo) != 0)
    {
        return "Error getting file info";
    }

    std::time_t modifiedTime = fileInfo.st_mtime;
    struct tm *timeinfo = std::localtime(&modifiedTime);

    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return buffer;
}

std::string getSystemFromRomPath(const char *fullpath)
{
    std::string arg_rom_string(fullpath);
    size_t slash = arg_rom_string.find_last_of("\\/");
    std::string dirPath = (slash != std::string::npos) ? arg_rom_string.substr(0, slash) : arg_rom_string;
    size_t slash2 = dirPath.find_last_of("\\/");
    std::string system = (slash2 != std::string::npos) ? dirPath.substr(slash2 + 1, dirPath.length()) : dirPath;
    logger.log(Logger::DEB, "system='%s'", system.c_str());
    return system;
}

std::string replace(std::string &str, const std::string &from, const std::string &to)
{
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
        return "";
    std::string replaced = str.replace(start_pos, from.length(), to);
    return replaced;
}

bool configValueIsTrue(const std::string &setting, bool fallback)
{
    const auto value = conf_map.find(setting);
    if (value == conf_map.end())
        return fallback;

    bool parsed = false;
    if (!rr::config::parseBoolean(value->second, parsed))
    {
        logger.log(Logger::WARN,
                   "Invalid boolean value '%s' for '%s'; using %s.",
                   value->second.c_str(), setting.c_str(),
                   fallback ? "true" : "false");
        return fallback;
    }
    return parsed;
}

std::string configValue(const std::string &setting, const std::string &fallback)
{
    const auto value = conf_map.find(setting);
    return value == conf_map.end() ? fallback : value->second;
}

int configValueInteger(const std::string &setting, int fallback,
                       int minimum, int maximum)
{
    const auto value = conf_map.find(setting);
    if (value == conf_map.end())
        return fallback;

    int parsed = 0;
    if (!rr::config::parseInteger(value->second, minimum, maximum, parsed))
    {
        logger.log(Logger::WARN,
                   "Invalid integer value '%s' for '%s'; expected %d..%d, using %d.",
                   value->second.c_str(), setting.c_str(),
                   minimum, maximum, fallback);
        return fallback;
    }
    return parsed;
}

bool persistConfigSetting(const std::string &setting, const std::string &value)
{
    std::ifstream input(activeConfigFile);
    if (!input.good()) return false;

    const std::string encodedValue = rr::config::encodeValue(value);
    std::vector<std::string> lines;
    std::string line;
    bool replaced = false;
    while (std::getline(input, line)) {
        std::string key = line.substr(0, line.find('='));
        trim(key);
        if (key == setting) {
            line = setting + (encodedValue.empty() ? " =" : " = " + encodedValue);
            replaced = true;
        }
        lines.push_back(line);
    }
    input.close();
    if (!replaced)
        lines.push_back(setting + (encodedValue.empty() ? " =" : " = " + encodedValue));

    const std::string temporary = activeConfigFile + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output.good()) return false;
    for (const std::string &entry : lines) output << entry << '\n';
    output.close();
    if (!output.good()) {
        std::remove(temporary.c_str());
        return false;
    }

    struct stat configStat = {};
    if (stat(activeConfigFile.c_str(), &configStat) == 0) {
        if (chmod(temporary.c_str(), configStat.st_mode & 07777) != 0)
            logger.log(Logger::WARN, "Unable to preserve permissions for '%s': %s",
                       temporary.c_str(), std::strerror(errno));
#ifndef _WIN32
        if (chown(temporary.c_str(), configStat.st_uid, configStat.st_gid) != 0 &&
            errno != EPERM)
            logger.log(Logger::WARN, "Unable to preserve ownership for '%s': %s",
                       temporary.c_str(), std::strerror(errno));
#endif
    }
#ifdef _WIN32
    std::remove(activeConfigFile.c_str());
#endif
    if (std::rename(temporary.c_str(), activeConfigFile.c_str()) != 0) {
        std::remove(temporary.c_str());
        return false;
    }
    conf_map[setting] = value;
    return true;
}

bool persistVideoSetting(const std::string &setting, const std::string &value)
{
    return persistConfigSetting(setting, value);
}

// --- initConfig ---

void initConfig()
{
    logger.log(Logger::INF, "Searching config file... ");

    std::string config_file;
    if (opt_setting_file_explicit)
    {
        config_file = opt_setting_file;
        logger.log(Logger::INF,
                   "Using command-line configuration file: '%s'",
                   config_file.c_str());
    }
    else if (fileExists("retrorun.cfg"))
    {
        config_file = "retrorun.cfg";
        logger.log(Logger::INF, "Using local configuration file: '%s'", config_file.c_str());
    }
    else
    {
        logger.log(Logger::INF, "Local configuration file not found. Using configuration file: '%s'", opt_setting_file);
        config_file = opt_setting_file;
    }
    activeConfigFile = getAbsolutePath(config_file);
    std::ifstream infile(activeConfigFile);

    if (!infile.good())
    {
        logger.log(Logger::ERR, "Configuration file:'%s' doesn't exist default core settings will be used", activeConfigFile.c_str());
    }
    else
    {
        logger.log(Logger::INF, "Configuration found, reading configuration file:'%s'", activeConfigFile.c_str());
        initMapConfig(activeConfigFile);

        bool logToFile = false;
        bool logFileReady = false;
        std::string logFilePath;
        const auto logToFileSetting = conf_map.find("retrorun_log_to_file");
        if (logToFileSetting != conf_map.end())
        {
            logToFile = configValueIsTrue("retrorun_log_to_file", false);
            if (logToFile)
            {
                const std::filesystem::path configLogPath =
                    std::filesystem::path(activeConfigFile).parent_path() / "retrorun.log";

                logFilePath = configLogPath.string();
                logFileReady = logger.enableFileLogging(logFilePath);
                if (!logFileReady)
                    logger.log(Logger::ERR, "Unable to open RetroRun log file: '%s'",
                               logFilePath.c_str());
            }
        }

        try
        {
            const std::string &arValue = conf_map.at("retrorun_log_level");
            logger.setLogLevel(getLogLevel(arValue));
            logger.log(Logger::INF, "retrorun_log_level: %s\n", arValue.c_str());
            logger.log(Logger::INF, "retrorun_config_file: '%s'", activeConfigFile.c_str());
        }
        catch (...)
        {
            logger.setLogLevel(Logger::INF);
            logger.log(Logger::INF, "retrorun_log_level parameter not found in retrorun.cfg using default value (INFO).");
            logger.log(Logger::INF, "retrorun_config_file: '%s'", activeConfigFile.c_str());
        }

        if (logToFile && logFileReady)
            logger.log(Logger::INF, "retrorun_log_to_file: true (file='%s')", logFilePath.c_str());
        else if (!logToFile)
            logger.log(Logger::DEB, "retrorun_log_to_file: false.");

        try
        {
            const std::string &coreLogValue = conf_map.at("retrorun_core_log_level");
            Logger::setCoreLogLevel(getLogLevel(coreLogValue));
            logger.log(Logger::INF, "retrorun_core_log_level: %s", coreLogValue.c_str());
        }
        catch (...)
        {
            Logger::setCoreLogLevel(Logger::ERR);
            logger.log(Logger::DEB, "retrorun_core_log_level parameter not found in retrorun.cfg using default value (ERROR).");
        }

#ifdef RR_HYBRID_AUDIO
        {
            const std::string requestedAudioBackend =
                configValue("retrorun_audio_backend", "auto");
            if (!rr_audio_backend_select(requestedAudioBackend.c_str())) {
                logger.log(Logger::WARN,
                           "Unknown or unavailable retrorun_audio_backend '%s'; using %s",
                           requestedAudioBackend.c_str(), rr_audio_backend_name());
                rr_audio_backend_select("auto");
            }
            logger.log(Logger::INF,
                       "retrorun_audio_backend: requested=%s, resolved=%s",
                       requestedAudioBackend.c_str(), rr_audio_backend_name());
        }
#endif

        try
        {
            const std::string &asValue = conf_map.at("retrorun_device_name");
            retrorun_device_name = asValue;
            resetDeviceName();
            logger.log(Logger::INF, "retrorun_device_name: %s.", retrorun_device_name.c_str());
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_device_name parameter not found in retrorun.cfg, device name will be detected in a different way..."); }

        try
        {
            const std::string &ssFolderValue = conf_map.at("retrorun_screenshot_folder");
            screenShotFolder = ssFolderValue;
            logger.log(Logger::DEB, "Screenshot folder:%s", screenShotFolder.c_str());
        }
        catch (...)
        {
#ifdef RR_PLATFORM_SDL
            logger.log(Logger::DEB, "retrorun_screenshot_folder parameter not found; using current directory.");
            screenShotFolder = ".";
#else
            logger.log(Logger::DEB, "retrorun_screenshot_folder parameter not found in retrorun.cfg using default folder (/storage/roms/screenshots).");
            screenShotFolder = "/storage/roms/screenshots";
#endif
        }

        try
        {
            conf_map.at("retrorun_fps_counter");
            input_fps_requested = configValueIsTrue("retrorun_fps_counter", false);
            logger.log(Logger::DEB, "retrorun_fps_counter :%s", input_fps_requested ? "TRUE" : "FALSE");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_fps_counter parameter not found in retrorun.cfg using defaulf value (disabled)."); input_fps_requested = false; }

#ifdef RR_PLATFORM_SDL
        try
        {
            const std::string &renderer = conf_map.at("retrorun_video_renderer");
            if (renderer == "auto") sdlVideoRenderer = SDLVideoRenderer::Auto;
            else if (renderer == "software") sdlVideoRenderer = SDLVideoRenderer::Software;
            else if (renderer == "opengl") sdlVideoRenderer = SDLVideoRenderer::OpenGL;
            else if (renderer == "vulkan") sdlVideoRenderer = SDLVideoRenderer::Vulkan;
            else logger.log(Logger::WARN, "Unknown retrorun_video_renderer '%s'; using auto", renderer.c_str());
            logger.log(Logger::DEB, "retrorun_video_renderer: %s", renderer.c_str());
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_video_renderer not found; using auto"); }

        try
        {
            conf_map.at("retrorun_vsync");
            sdlVsync = configValueIsTrue("retrorun_vsync", false);
            logger.log(Logger::DEB, "retrorun_vsync: %s", sdlVsync ? "true" : "false");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_vsync not found; using false"); }
        rr_video_vsync_set(sdlVsync);
#endif

        try
        {
            const std::string &value = conf_map.at("retrorun_video_filter");
            if (value == "nearest") videoFilter = RR_VIDEO_FILTER_NEAREST;
            else if (value == "linear") videoFilter = RR_VIDEO_FILTER_LINEAR;
            else if (value == "off" || value == "default") videoFilter = RR_VIDEO_FILTER_DEFAULT;
            else logger.log(Logger::WARN, "Unknown retrorun_video_filter '%s'; using off", value.c_str());
            logger.log(Logger::DEB, "retrorun_video_filter: %s", value.c_str());
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_video_filter not found; using off"); }
        rr_video_filter_set(videoFilter);

        try
        {
            const std::string &value = conf_map.at("retrorun_video_shader");
            if (value == "scanlines") videoShader = RR_VIDEO_SHADER_SCANLINES;
            else if (value == "crt") videoShader = RR_VIDEO_SHADER_CRT;
            else if (value == "off" || value == "none") videoShader = RR_VIDEO_SHADER_OFF;
            else logger.log(Logger::WARN, "Unknown retrorun_video_shader '%s'; using off", value.c_str());
            logger.log(Logger::DEB, "retrorun_video_shader: %s", value.c_str());
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_video_shader not found; using off"); }
        rr_video_shader_set(videoShader);

        if (opt_aspect != 0.0f)
        {
            logger.log(Logger::DEB, "aspect_ratio forced from command line.");
        }
        else
        {
            try
            {
                const std::string &arValue = conf_map.at("retrorun_aspect_ratio");
                opt_aspect = getAspectRatio(arValue);
                logger.log(Logger::DEB, "retrorun_aspect_ratio :%f", opt_aspect);
            }
            catch (...) { logger.log(Logger::DEB, "retrorun_aspect_ratio parameter not found in retrorun.cfg using default value (core provided)."); }
        }

        try
        {
            conf_map.at("retrorun_auto_save");
            auto_save = configValueIsTrue("retrorun_auto_save", auto_save);
            logger.log(Logger::DEB, "retrorun_auto_save: %s.", auto_save ? "true" : "false");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_auto_save parameter not found in retrorun.cfg using default value (%s).", auto_save ? "true" : "false"); }

        try
        {
            conf_map.at("retrorun_auto_load");
            auto_load = configValueIsTrue("retrorun_auto_load", auto_save);
            logger.log(Logger::DEB, "retrorun_auto_load: %s.", auto_load ? "true" : "false");
        }
        catch (...)
        {
            logger.log(Logger::DEB, "By defualt retrorun auto_load will be the same as auto_save.");
            auto_load = auto_save;
            logger.log(Logger::DEB, "retrorun_auto_load parameter not found in retrorun.cfg using default value (%s).", auto_load ? "true" : "false");
        }

        const auto analogModeSetting = conf_map.find("retrorun_analog_to_digital");
        if (analogModeSetting != conf_map.end())
        {
            if (!setAnalogToDigitalMode(analogModeSetting->second))
                logger.log(Logger::WARN,
                           "Invalid retrorun_analog_to_digital value '%s'; using %s.",
                           analogModeSetting->second.c_str(),
                           analogToDigitalModeName(analogToDigital));
            else
                logger.log(Logger::DEB, "retrorun_analog_to_digital: %s.",
                           analogToDigitalModeName(analogToDigital));
        }
        else
        {
            // Backward compatibility: the old boolean always forced the left
            // stick mapping and disabled native analog input.
            const auto legacyAnalogSetting = conf_map.find("retrorun_force_left_analog_stick");
            if (legacyAnalogSetting != conf_map.end())
                setAnalogToDigitalMode(configValueIsTrue(
                                           "retrorun_force_left_analog_stick", false)
                                           ? "left_forced" : "none");
            logger.log(Logger::DEB,
                       "retrorun_analog_to_digital parameter not found; using %s%s.",
                       analogToDigitalModeName(analogToDigital),
                       legacyAnalogSetting != conf_map.end() ? " from legacy setting" : " by default");
        }

        try
        {
            conf_map.at("retrorun_loop_declared_fps");
            runLoopAtDeclaredfps = configValueIsTrue(
                "retrorun_loop_declared_fps", runLoopAtDeclaredfps);
            logger.log(Logger::DEB, "retrorun_loop_declared_fps: %s.", runLoopAtDeclaredfps ? "true" : "false");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_loop_declared_fps parameter not found in retrorun.cfg using default value (%s).", runLoopAtDeclaredfps ? "true" : "false"); }

        try
        {
            conf_map.at("retrorun_swap_l1r1_with_l2r2");
            swapL1R1WithL2R2 = configValueIsTrue(
                "retrorun_swap_l1r1_with_l2r2", swapL1R1WithL2R2);
            logger.log(Logger::DEB, "retrorun_swap_l1r1_with_l2r2: %s.", swapL1R1WithL2R2 ? "true" : "false");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_swap_l1r1_with_l2r2 parameter not found in retrorun.cfg using default value (%s).", swapL1R1WithL2R2 ? "true" : "false"); }

        try
        {
            conf_map.at("retrorun_swap_sticks");
            swapSticks = configValueIsTrue("retrorun_swap_sticks", swapSticks);
            logger.log(Logger::DEB, "retrorun_swap_sticks: %s.", swapSticks ? "true" : "false");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_swap_sticks parameter not found in retrorun.cfg using default value (%s).", swapSticks ? "true" : "false"); }

        try
        {
            conf_map.at("retrorun_audio_buffer");
            retrorun_audio_buffer = configValueInteger(
                "retrorun_audio_buffer", retrorun_audio_buffer, -1, 65536);
            new_retrorun_audio_buffer = retrorun_audio_buffer;
            logger.log(Logger::DEB, "retrorun_audio_buffer: %d.", retrorun_audio_buffer);
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_audio_buffer parameter not found in retrorun.cfg using default value (-1)."); }

        try
        {
            conf_map.at("retrorun_audio_stable_buffer");
            retrorun_audio_stable_buffer = configValueIsTrue(
                "retrorun_audio_stable_buffer", retrorun_audio_stable_buffer);
            logger.log(Logger::DEB, "retrorun_audio_stable_buffer: %s.",
                       retrorun_audio_stable_buffer ? "true" : "false");
        }
        catch (...)
        {
            logger.log(Logger::DEB,
                       "retrorun_audio_stable_buffer parameter not found; using false.");
        }

        try
        {
            conf_map.at("retrorun_sdl_audio_stretch_percent");
            retrorun_sdl_audio_stretch_percent = configValueInteger(
                "retrorun_sdl_audio_stretch_percent", 0, 0, 10);
            logger.log(Logger::DEB,
                       "retrorun_sdl_audio_stretch_percent: %d.",
                       retrorun_sdl_audio_stretch_percent);
        }
        catch (...)
        {
            retrorun_sdl_audio_stretch_percent = 0;
            logger.log(Logger::DEB,
                       "retrorun_sdl_audio_stretch_percent parameter not found or invalid; using 0.");
        }

        try
        {
            conf_map.at("retrorun_sdl_audio_stretch_low_ms");
            retrorun_sdl_audio_stretch_low_ms = configValueInteger(
                "retrorun_sdl_audio_stretch_low_ms", 40, 0, 200);
            logger.log(Logger::DEB,
                       "retrorun_sdl_audio_stretch_low_ms: %d.",
                       retrorun_sdl_audio_stretch_low_ms);
        }
        catch (...)
        {
            retrorun_sdl_audio_stretch_low_ms = 40;
            logger.log(Logger::DEB,
                       "retrorun_sdl_audio_stretch_low_ms parameter not found or invalid; using 40.");
        }

        try
        {
            conf_map.at("retrorun_go2_audio_stretch_percent");
            retrorun_go2_audio_stretch_percent = configValueInteger(
                "retrorun_go2_audio_stretch_percent", 0, 0, 10);
            logger.log(Logger::DEB,
                       "retrorun_go2_audio_stretch_percent: %d.",
                       retrorun_go2_audio_stretch_percent);
        }
        catch (...)
        {
            retrorun_go2_audio_stretch_percent = 0;
            logger.log(Logger::DEB,
                       "retrorun_go2_audio_stretch_percent parameter not found or invalid; using 0.");
        }

        try
        {
            conf_map.at("retrorun_go2_audio_stretch_low_ms");
            retrorun_go2_audio_stretch_low_ms = configValueInteger(
                "retrorun_go2_audio_stretch_low_ms", 40, 0, 200);
            logger.log(Logger::DEB,
                       "retrorun_go2_audio_stretch_low_ms: %d.",
                       retrorun_go2_audio_stretch_low_ms);
        }
        catch (...)
        {
            retrorun_go2_audio_stretch_low_ms = 40;
            logger.log(Logger::DEB,
                       "retrorun_go2_audio_stretch_low_ms parameter not found or invalid; using 40.");
        }

        try
        {
            conf_map.at("retrorun_force_audio_multithread");
            forceAudioMultithread = configValueIsTrue(
                "retrorun_force_audio_multithread", forceAudioMultithread);
            logger.log(Logger::DEB, "retrorun_force_audio_multithread: %s.",
                       forceAudioMultithread ? "true" : "false");
        }
        catch (...)
        {
            logger.log(Logger::DEB,
                       "retrorun_force_audio_multithread parameter not found; using false.");
        }

        try
        {
            conf_map.at("retrorun_mouse_speed_factor");
            retrorun_mouse_speed_factor = configValueInteger(
                "retrorun_mouse_speed_factor", retrorun_mouse_speed_factor, 1, 100);
            logger.log(Logger::DEB, "retrorun_mouse_speed_factor: %d.", retrorun_mouse_speed_factor);
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_mouse_speed_factor parameter not found in retrorun.cfg using default value (5)."); }

        try
        {
            const std::string &arValue = conf_map.at("retrorun_tate_mode");
            tateState = getTateMode(arValue);
            logger.log(Logger::DEB, "retrorun_tate_mode :%f\n", opt_aspect);
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_tate_mode parameter not found in retrorun.cfg using default value (DISABLED).\n"); }

        try
        {
            conf_map.at("retrorun_disable_rumble");
            disableRumble = configValueIsTrue(
                "retrorun_disable_rumble", disableRumble);
            logger.log(Logger::DEB, "retrorun_disable_rumble: %s.", disableRumble ? "true" : "false");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_disable_rumble parameter not found in retrorun.cfg using default value (%s).", disableRumble ? "true" : "false"); }

        int rumble_type_pwm = TRIBOOL_NULL;
        try
        {
            const std::string &asValue = conf_map.at("retrorun_rumble_type");
            if (asValue == "pwm") {
                rumble_type_pwm = TRIBOOL_TRUE;
                logger.log(Logger::DEB, "retrorun_rumble_type: %s.", asValue.c_str());
            } else if (asValue == "event") {
                rumble_type_pwm = TRIBOOL_FALSE;
                logger.log(Logger::DEB, "retrorun_rumble_type: %s.", asValue.c_str());
            } else {
                logger.log(Logger::DEB, "retrorun_rumble_type parameter not recognized (possible values are: 'PWM', 'EVENT' ) in retrorun.cfg using default value PWM.");
            }
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_rumble_type parameter not found in retrorun.cfg using default value PWM."); }

        try
        {
            const std::string &ssFolderValue = conf_map.at("retrorun_rumble_pwm_file");
            PWM_RUMBLE_PATH = ssFolderValue;
            logger.log(Logger::DEB, "retrorun_rumble_pwm_file set to:%s", PWM_RUMBLE_PATH.c_str());
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_rumble_pwm_file parameter not found in retrorun.cfg using default value (%s).", PWM_RUMBLE_PATH.c_str()); }

        logger.log(Logger::DEB, "DEVICE_PATH: (%s).", DEVICE_PATH.c_str());
        try
        {
            const std::string &ssFolderValue = conf_map.at("retrorun_rumble_event");
            DEVICE_PATH = ssFolderValue;
            logger.log(Logger::DEB, "retrorun_rumble_event set to:%s", DEVICE_PATH.c_str());
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_rumble_event parameter not found in retrorun.cfg using default value (%s).", DEVICE_PATH.c_str()); }

        try
        {
            conf_map.at("retrorun_alternative_input_mode");
            input_info_requested_alternative = configValueIsTrue(
                "retrorun_alternative_input_mode",
                input_info_requested_alternative);
            logger.log(Logger::DEB, "retrorun_alternative_input_mode: %s.", input_info_requested_alternative ? "true" : "false");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_alternative_input_mode parameter not found in retrorun.cfg using default value (%s).", input_info_requested_alternative ? "true" : "false"); }

        try
        {
            conf_map.at("retrorun_force_video_multithread");
            forceVideoMultithread = configValueIsTrue(
                "retrorun_force_video_multithread", forceVideoMultithread);
            logger.log(Logger::DEB, "retrorun_force_video_multithread: %s.", forceVideoMultithread ? "true" : "false");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_force_video_multithread parameter not found in retrorun.cfg using default value."); }

        try
        {
            conf_map.at("retrorun_drm_direct_scanout");
            drmDirectScanoutMode = configValueIsTrue(
                "retrorun_drm_direct_scanout", false)
                    ? DRMDirectScanoutMode::Enabled
                    : DRMDirectScanoutMode::Disabled;
            const char *mode = drmDirectScanoutMode == DRMDirectScanoutMode::Enabled
                ? "true" : "false";
            logger.log(Logger::DEB, "retrorun_drm_direct_scanout: %s.", mode);
        }
        catch (...)
        {
            logger.log(Logger::DEB,
                       "retrorun_drm_direct_scanout parameter not found; using false.");
        }

        try
        {
            conf_map.at("retrorun_adaptive_frameskip");
            adaptiveFrameSkip = configValueIsTrue(
                "retrorun_adaptive_frameskip", adaptiveFrameSkip);
            logger.log(Logger::DEB, "retrorun_adaptive_frameskip: %s.", adaptiveFrameSkip ? "true" : "false");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_adaptive_frameskip parameter not found; using %s.", adaptiveFrameSkip ? "true" : "false"); }

        try
        {
            conf_map.at("retrorun_frameskip");
            fixedFrameSkip = configValueInteger(
                "retrorun_frameskip", 0, 0, 5);
            logger.log(Logger::DEB, "retrorun_frameskip: %d.", fixedFrameSkip);
        }
        catch (...)
        {
            fixedFrameSkip = 0;
            logger.log(Logger::DEB, "retrorun_frameskip parameter not found; using 0.");
        }

        if (fixedFrameSkip > 0 && adaptiveFrameSkip)
        {
            adaptiveFrameSkip = false;
            logger.log(Logger::WARN, "Fixed frameskip is enabled; adaptive frameskip has been disabled.");
        }

        try
        {
            conf_map.at("retrorun_enable_key_log");
            enable_key_log = configValueIsTrue(
                "retrorun_enable_key_log", enable_key_log);
            logger.log(Logger::DEB, "retrorun_enable_key_log: %s.", enable_key_log ? "true" : "false");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_enable_key_log parameter not found in retrorun.cfg using default value (%s)", enable_key_log ? "true" : "false"); }

        try
        {
            const std::string &ssFolderValue = conf_map.at("retrorun_extra_retrogame_name");
            events::extra_retrogame_name = ssFolderValue;
            logger.log(Logger::DEB, "retrorun_extra_retrogame_name set to:%s", events::extra_retrogame_name.c_str());
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_extra_retrogame_name parameter not found in retrorun.cfg using default values."); }

        try
        {
            const std::string &ssFolderValue = conf_map.at("retrorun_extra_osh_name");
            events::extra_osh_name = ssFolderValue;
            logger.log(Logger::DEB, "retrorun_extra_osh_name set to:%s", events::extra_osh_name.c_str());
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_extra_osh_name parameter not found in retrorun.cfg using default values."); }

        try
        {
            const std::string &ssFolderValue = conf_map.at("retrorun_extra_evdev_name");
            events::extra_evdev_name = ssFolderValue;
            logger.log(Logger::DEB, "retrorun_extra_evdev_name set to:%s", events::extra_evdev_name.c_str());
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_extra_evdev_name parameter not found in retrorun.cfg using default values."); }

        try
        {
            conf_map.at("retrorun_show_loading_screen");
            showLoading = configValueIsTrue(
                "retrorun_show_loading_screen", showLoading);
            logger.log(Logger::DEB, "retrorun_show_loading_screen: %s.", showLoading ? "true" : "false");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_show_loading_screen parameter not found in retrorun.cfg using default value (%s)", showLoading ? "true" : "false"); }

        try
        {
            const std::string &value = conf_map.at("retrorun_ui_profile");
            if (value == "auto") setUIProfile(UIProfile::Auto);
            else if (value == "handheld") setUIProfile(UIProfile::Handheld);
            else if (value == "desktop") setUIProfile(UIProfile::Desktop);
            else logger.log(Logger::WARN, "Unknown retrorun_ui_profile '%s'; using auto", value.c_str());
            logger.log(Logger::DEB, "retrorun_ui_profile: %s", value.c_str());
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_ui_profile parameter not found; using auto"); }

        try
        {
            conf_map.at("retrorun_pixel_perfect");
            pixel_perfect = configValueIsTrue(
                "retrorun_pixel_perfect", pixel_perfect);
            logger.log(Logger::DEB, "retrorun_pixel_perfect: %s.", pixel_perfect ? "true" : "false");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_pixel_perfect parameter not found in retrorun.cfg using default value (%s)", pixel_perfect ? "true" : "false"); }

        pwm = rumble_type_pwm;

        adaptiveFps = false;
        logger.log(Logger::DEB, "Configuration initialized.");
    }

    infile.close();
}
