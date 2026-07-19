/*
retrorun - libretro frontend for Anbernic Devices
Copyright (C) 2020  OtherCrashOverride
Copyright (C) 2021-present  navy1978
*/

#include "config.h"
#include "globals.h"
#include "video-helper.h"
#include "input.h"
#include "rumble.h"
#include "platform.h"
#include "./js2xbox/events.h"

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

bool opt_show_fps = false;
bool auto_save = false;
bool auto_load = false;

rr_video_filter_t videoFilter = RR_VIDEO_FILTER_DEFAULT;
rr_video_shader_t videoShader = RR_VIDEO_SHADER_OFF;

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
    std::ifstream file_in(pathConfFile);

    std::string key;
    std::string value;

    while (std::getline(file_in, key, '=') && std::getline(file_in, value))
    {
        try
        {
            std::size_t pos_sharp = key.find("#");
            if (pos_sharp == 0)
            {
                key = key.substr(key.find("\n") + 1, key.length());
                std::istringstream iss(key);
                std::getline(iss, key, '=');
                std::getline(iss, value);
            }
            key = trim(key);
            value = trim(value);
            conf_map.insert(std::pair<std::string, std::string>(key, value));
        }
        catch (...)
        {
            logger.log(Logger::ERR, "Error reading configuration file, key:%s", key.c_str());
        }
    }
    logger.log(Logger::DEB, "Configuration loaded!");
    file_in.close();
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
    return value->second == "true" || value->second == "enabled" || value->second == "1";
}

std::string configValue(const std::string &setting, const std::string &fallback)
{
    const auto value = conf_map.find(setting);
    return value == conf_map.end() ? fallback : value->second;
}

bool persistConfigSetting(const std::string &setting, const std::string &value)
{
    std::ifstream input(activeConfigFile);
    if (!input.good()) return false;

    std::vector<std::string> lines;
    std::string line;
    bool replaced = false;
    while (std::getline(input, line)) {
        std::string key = line.substr(0, line.find('='));
        trim(key);
        if (key == setting) {
            line = setting + (value.empty() ? " =" : " = " + value);
            replaced = true;
        }
        lines.push_back(line);
    }
    input.close();
    if (!replaced)
        lines.push_back(setting + (value.empty() ? " =" : " = " + value));

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

    std::string config_file = "retrorun.cfg";

    if (fileExists(config_file.c_str()))
    {
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
            const std::string &value = logToFileSetting->second;
            logToFile = value == "true" || value == "enabled" || value == "1";
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
            const std::string &ssFps_counter = conf_map.at("retrorun_fps_counter");
            input_fps_requested = ssFps_counter == "true" || ssFps_counter == "enabled" ||
                                  ssFps_counter == "1";
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
            const std::string &value = conf_map.at("retrorun_vsync");
            sdlVsync = value == "true" || value == "enabled" || value == "1";
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
            const std::string &asValue = conf_map.at("retrorun_auto_save");
            auto_save = asValue == "true" ? true : false;
            logger.log(Logger::DEB, "retrorun_auto_save: %s.", auto_save ? "true" : "false");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_auto_save parameter not found in retrorun.cfg using default value (%s).", auto_save ? "true" : "false"); }

        try
        {
            const std::string &asValue = conf_map.at("retrorun_auto_load");
            auto_load = asValue == "true" ? true : false;
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
                setAnalogToDigitalMode(legacyAnalogSetting->second == "true"
                                           ? "left_forced" : "none");
            logger.log(Logger::DEB,
                       "retrorun_analog_to_digital parameter not found; using %s%s.",
                       analogToDigitalModeName(analogToDigital),
                       legacyAnalogSetting != conf_map.end() ? " from legacy setting" : " by default");
        }

        try
        {
            const std::string &tflValue = conf_map.at("retrorun_loop_declared_fps");
            runLoopAtDeclaredfps = tflValue == "false" ? false : true;
            logger.log(Logger::DEB, "retrorun_loop_declared_fps: %s.", runLoopAtDeclaredfps ? "true" : "false");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_loop_declared_fps parameter not found in retrorun.cfg using default value (%s).", runLoopAtDeclaredfps ? "true" : "false"); }

        try
        {
            const std::string &asValue = conf_map.at("retrorun_swap_l1r1_with_l2r2");
            swapL1R1WithL2R2 = asValue == "true" ? true : false;
            logger.log(Logger::DEB, "retrorun_swap_l1r1_with_l2r2: %s.", swapL1R1WithL2R2 ? "true" : "false");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_swap_l1r1_with_l2r2 parameter not found in retrorun.cfg using default value (%s).", swapL1R1WithL2R2 ? "true" : "false"); }

        try
        {
            const std::string &asValue = conf_map.at("retrorun_swap_sticks");
            swapSticks = asValue == "true" ? true : false;
            logger.log(Logger::DEB, "retrorun_swap_sticks: %s.", swapSticks ? "true" : "false");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_swap_sticks parameter not found in retrorun.cfg using default value (%s).", swapSticks ? "true" : "false"); }

        try
        {
            const std::string &audioBufferValue = conf_map.at("retrorun_audio_buffer");
            if (!audioBufferValue.empty())
            {
                retrorun_audio_buffer = stoi(audioBufferValue);
                new_retrorun_audio_buffer = retrorun_audio_buffer;
                logger.log(Logger::DEB, "retrorun_audio_buffer: %d.", retrorun_audio_buffer);
            }
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_audio_buffer parameter not found in retrorun.cfg using default value (-1)."); }

        try
        {
            const std::string &value = conf_map.at("retrorun_audio_stable_buffer");
            retrorun_audio_stable_buffer = value == "true";
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
            const std::string &value = conf_map.at("retrorun_force_audio_multithread");
            forceAudioMultithread = value == "true" || value == "enabled" || value == "1";
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
            const std::string &mouseSpeedValue = conf_map.at("retrorun_mouse_speed_factor");
            if (!mouseSpeedValue.empty())
            {
                retrorun_mouse_speed_factor = stoi(mouseSpeedValue);
                logger.log(Logger::DEB, "retrorun_mouse_speed_factor: %d.", retrorun_mouse_speed_factor);
            }
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
            const std::string &lasValue = conf_map.at("retrorun_disable_rumble");
            disableRumble = lasValue == "true" ? true : false;
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
            const std::string &asValue = conf_map.at("retrorun_alternative_input_mode");
            input_info_requested_alternative = asValue == "true" ? true : false;
            logger.log(Logger::DEB, "retrorun_alternative_input_mode: %s.", input_info_requested_alternative ? "true" : "false");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_alternative_input_mode parameter not found in retrorun.cfg using default value (%s).", input_info_requested_alternative ? "true" : "false"); }

        try
        {
            const std::string &asValue = conf_map.at("retrorun_force_video_multithread");
            forceVideoMultithread = asValue == "true";
            logger.log(Logger::DEB, "retrorun_force_video_multithread: %s.", forceVideoMultithread ? "true" : "false");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_force_video_multithread parameter not found in retrorun.cfg using default value."); }

        try
        {
            const std::string &value = conf_map.at("retrorun_drm_direct_scanout");
            if (value == "auto")
                drmDirectScanoutMode = DRMDirectScanoutMode::Auto;
            else if (value == "true" || value == "enabled" || value == "1")
                drmDirectScanoutMode = DRMDirectScanoutMode::Enabled;
            else if (value == "false" || value == "disabled" || value == "0")
                drmDirectScanoutMode = DRMDirectScanoutMode::Disabled;
            else
            {
                drmDirectScanoutMode = DRMDirectScanoutMode::Auto;
                logger.log(Logger::WARN,
                           "Unknown retrorun_drm_direct_scanout value '%s'; using auto.",
                           value.c_str());
            }
            const char *mode = drmDirectScanoutMode == DRMDirectScanoutMode::Auto ? "auto" :
                (drmDirectScanoutMode == DRMDirectScanoutMode::Enabled ? "true" : "false");
            logger.log(Logger::DEB, "retrorun_drm_direct_scanout: %s.", mode);
        }
        catch (...)
        {
            logger.log(Logger::DEB,
                       "retrorun_drm_direct_scanout parameter not found; using auto.");
        }

        try
        {
            const std::string &value = conf_map.at("retrorun_adaptive_frameskip");
            adaptiveFrameSkip = value == "true";
            logger.log(Logger::DEB, "retrorun_adaptive_frameskip: %s.", adaptiveFrameSkip ? "true" : "false");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_adaptive_frameskip parameter not found; using %s.", adaptiveFrameSkip ? "true" : "false"); }

        try
        {
            const std::string &value = conf_map.at("retrorun_frameskip");
            fixedFrameSkip = std::max(0, std::min(5, stoi(value)));
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
            const std::string &asValue = conf_map.at("retrorun_enable_key_log");
            enable_key_log = asValue == "true" ? true : false;
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
            const std::string &asValue = conf_map.at("retrorun_show_loading_screen");
            showLoading = asValue == "true" ? true : false;
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
            const std::string &asValue = conf_map.at("retrorun_pixel_perfect");
            pixel_perfect = asValue == "true" ? true : false;
            logger.log(Logger::DEB, "retrorun_pixel_perfect: %s.", pixel_perfect ? "true" : "false");
        }
        catch (...) { logger.log(Logger::DEB, "retrorun_pixel_perfect parameter not found in retrorun.cfg using default value (%s)", pixel_perfect ? "true" : "false"); }

        pwm = rumble_type_pwm;

        adaptiveFps = false;
        logger.log(Logger::DEB, "Configuration initialized.");
    }

    infile.close();
}
