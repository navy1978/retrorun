#pragma once

/*
retrorun - libretro frontend for Anbernic Devices
Copyright (C) 2020  OtherCrashOverride
Copyright (C) 2021-present  navy1978
*/

#include <string>
#include "logger.h"
#include "platform.h"
#include "globals.h"

// String utilities
std::string &rtrim(std::string &s);
std::string &ltrim(std::string &s);
std::string &trim(std::string &s);

// Config file parsing
void initMapConfig(std::string pathConfFile);
void initConfig();

// Config helpers
float getAspectRatio(const std::string aspect);
TateState getTateMode(const std::string tate);
Logger::LogLevel getLogLevel(const std::string level);
bool fileExists(const char *path);
std::string getLastModifiedTime(const char *path);
std::string getSystemFromRomPath(const char *fullpath);
std::string replace(std::string &str, const std::string &from, const std::string &to);

// Persist a setting back to the active config file and update conf_map.
bool persistConfigSetting(const std::string &setting, const std::string &value);

// Backward-compatible alias kept for existing callers.
bool persistVideoSetting(const std::string &setting, const std::string &value);

bool configValueIsTrue(const std::string &setting, bool fallback = false);
std::string configValue(const std::string &setting, const std::string &fallback = "");

// Active config file path (set during initConfig)
extern std::string activeConfigFile;

// SDL-specific settings (only relevant on SDL builds)
#ifdef RR_PLATFORM_SDL
enum class SDLVideoRenderer { Auto = 0, Software, OpenGL, Vulkan };
extern SDLVideoRenderer sdlVideoRenderer;
extern bool sdlVsync;
#endif

extern const char *opt_setting_file;
extern bool opt_show_fps;
extern bool auto_save;
extern bool auto_load;

extern rr_video_filter_t videoFilter;
extern rr_video_shader_t videoShader;
