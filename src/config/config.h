#pragma once

/*
retrorun - libretro frontend for Anbernic Devices
Copyright (C) 2020  OtherCrashOverride
Copyright (C) 2021-present  navy1978
*/

#include <string>
#include <map>
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

// Merge non-persistent per-content values after initConfig(). Core options are
// consumed later through GET_VARIABLE; the small set of frontend values used
// by the Flycast catalog is also reflected into its runtime globals.
void applyTransientConfigOverrides(
    const std::map<std::string, std::string> &overrides);

// Report core-option changes once through RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE.
bool consumeCoreVariablesUpdated();

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
int configValueInteger(const std::string &setting, int fallback,
                       int minimum, int maximum);

// Active config file path (set during initConfig)
extern std::string activeConfigFile;

// SDL-specific settings (only relevant on SDL builds)
#ifdef RR_PLATFORM_SDL
enum class SDLVideoRenderer { Auto = 0, Software, OpenGL, Vulkan };
extern SDLVideoRenderer sdlVideoRenderer;
extern bool sdlVsync;
#endif

extern const char *opt_setting_file;
extern bool opt_setting_file_explicit;
extern bool opt_show_fps;
extern bool auto_save;
extern bool auto_load;

extern rr_video_filter_t videoFilter;
extern rr_video_shader_t videoShader;
