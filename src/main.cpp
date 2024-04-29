/*
retrorun-go2 - libretro frontend for the ODROID-GO Advance
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
#include "video.h"
#include "audio.h"
#include "input.h"
#include <functional>

#include <unistd.h>   // Per la funzione access()
#include <cstdio>     // Per std::printf
#include <cstdlib>    // Per std::free
#include <sys/stat.h> // Per le funzioni stat()
#include <ctime>      // Per le funzioni localtime() e strftime()

#include <unistd.h>

// #include <go2/queue.h>

#include <linux/dma-buf.h>
#include <sys/ioctl.h>

#include "libretro.h"
#include <dlfcn.h>
#include <cstdarg>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <exception>
#include <getopt.h>
#include <map>
#include <vector>
#include <regex>

#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <drm/drm_fourcc.h>
#include <sys/time.h>
#include <go2/input.h>

#include <signal.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <chrono>
#include <thread>

#define RETRO_DEVICE_ATARI_JOYSTICK RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
#define RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER 56
#define ENABLE_VIDEO 1
#define ENABLE_AUDIO 2
#define USE_FAST_SAVESTATES 4
#define HARD_DISABLE_AUDIO 8
/* unsigned * --
 *
 * Allows an implementation to ask frontend preferred hardware
 * context to use. Core should use this information to deal
 * with what specific context to request with SET_HW_RENDER.
 *
 * 'data' points to an unsigned variable
 */

// extern go2_battery_state_t batteryState;
extern go2_brightness_state_t brightnessState;

retro_hw_context_reset_t retro_context_reset;

const char *opt_savedir = ".";
const char *opt_systemdir = ".";

int opt_backlight = -1;
int opt_volume = -1;
bool opt_restart = false;
const char *arg_core = "";
const char *arg_rom = "";

typedef std::map<std::string, std::string> varmap_t;
varmap_t variables;
int exitFlag = -1;
const char *opt_setting_file = "/storage/.config/distribution/configs/retrorun.cfg";
std::map<std::string, std::string> conf_map;
bool opt_show_fps = false;
bool auto_save = false;
bool auto_load = false;
const char *ws = " \t\n\r\f\v";

bool isRunning = true;

struct option longopts[] = {
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

static struct
{
    void *handle;
    bool initialized;

    void (*retro_init)(void);
    void (*retro_deinit)(void);
    unsigned (*retro_api_version)(void);
    void (*retro_get_system_info)(struct retro_system_info *info);
    void (*retro_get_system_av_info)(struct retro_system_av_info *info);
    void (*retro_set_controller_port_device)(unsigned port, unsigned device);
    void (*retro_reset)(void);
    void (*retro_run)(void);
    size_t (*retro_serialize_size)(void);
    bool (*retro_serialize)(void *data, size_t size);
    bool (*retro_unserialize)(const void *data, size_t size);
    //	void retro_cheat_reset(void);
    //	void retro_cheat_set(unsigned index, bool enabled, const char *code);
    bool (*retro_load_game)(const struct retro_game_info *game);
    // bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info);
    void (*retro_unload_game)(void);
    //	unsigned retro_get_region(void);
    void *(*retro_get_memory_data)(unsigned id);
    size_t (*retro_get_memory_size)(unsigned id);
} g_retro;

#define load_sym(V, S)                                                         \
    do                                                                         \
    {                                                                          \
        if (!((*(void **)&V) = dlsym(g_retro.handle, #S)))                     \
        {                                                                      \
            printf("[noarch] Failed to load symbol '" #S "'': %s", dlerror()); \
            abort();                                                           \
        }                                                                      \
    } while (0)

#define load_retro_sym(S) load_sym(g_retro.S, S)

// using namespace std;
// using namespace std::chrono;

// trim from end of string (right)
inline std::string &rtrim(std::string &s)
{
    s.erase(s.find_last_not_of(ws) + 1);
    return s;
}

// trim from beginning of string (left)
inline std::string &ltrim(std::string &s)
{
    s.erase(0, s.find_first_not_of(ws));
    return s;
}

// trim from both ends of string (right then left)
inline std::string &trim(std::string &s)
{
    return ltrim(rtrim(s));
}

/**
 * Read a config file passed as parameter and create a map with all entries < key = value >
 * */

void initMapConfig(std::string pathConfFile)
{
    std::ifstream file_in(pathConfFile);
    // std::cout << file_in.rdbuf(); // debug

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
            // printf("Map values: key:%s ==> value:%s\n", key.c_str(), value.c_str());
            conf_map.insert(std::pair<std::string, std::string>(key, value));
        }
        catch (...)
        {
            logger.log(Logger::ERR, "Error reading configuration file, key:%s", key);
        }
    }
    logger.log(Logger::INF, "Configuration loaded!");

    // std::cout << "After init ====>mymap.size() is " << conf_map.size() << '\n';
    file_in.close();
}

static __eglMustCastToProperFunctionPointerType get_proc_address(const char *sym)
{
    __eglMustCastToProperFunctionPointerType result = eglGetProcAddress(sym);
    logger.log(Logger::DEB, "get_proc_address: sym='%s', result=%p", sym, (void *)result);

    return result;
}

/*static std::string trim(std::string str)
{
    return regex_replace(str, std::regex("(^[ ]+)|([ ]+$)"), "");
}*/

static bool core_environment(unsigned cmd, void *data)
{
    bool *bval;
    switch (cmd)
    {
    case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
        bval = (bool *)data;
        *bval = false;
        return true;

    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
    {
        struct retro_log_callback *cb = (struct retro_log_callback *)data;
        cb->log = Logger::core_log;
        break;
    }

    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        bval = (bool *)data;
        *bval = true;
        break;

    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
    {
        const enum retro_pixel_format fmt = *(enum retro_pixel_format *)data;

        switch (fmt)
        {
        case RETRO_PIXEL_FORMAT_0RGB1555:
            logger.log(Logger::DEB, "RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: DRM_FORMAT_RGBA5551");
            color_format = DRM_FORMAT_RGBA5551;
            break;

        case RETRO_PIXEL_FORMAT_RGB565:
            logger.log(Logger::DEB, "RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: DRM_FORMAT_RGB565");
            color_format = DRM_FORMAT_RGB565;
            break;

        case RETRO_PIXEL_FORMAT_XRGB8888:
            logger.log(Logger::DEB, "RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: DRM_FORMAT_XRGB8888");
            color_format = DRM_FORMAT_XRGB8888;
            break;

        default:
            logger.log(Logger::DEB, "RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: FORMAT UNKNOWN");
            return false;
        }

        return true;
    }

    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *(const char **)data = opt_systemdir;
        return true;

    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *(const char **)data = opt_savedir;
        return true;

    case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER:
    {
        logger.log(Logger::DEB, "RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: preferred OPENGLES3");
        unsigned int *preferred = (unsigned int *)data;
        *preferred = RETRO_HW_CONTEXT_OPENGLES3;
        return true;
    }

    case RETRO_ENVIRONMENT_SET_HW_RENDER:
    {
        retro_hw_render_callback *hw = (retro_hw_render_callback *)data;
        logger.log(Logger::DEB, "RETRO_ENVIRONMENT_SET_HW_RENDER: context_type=%d", hw->context_type);

        if (hw->context_type != RETRO_HW_CONTEXT_OPENGLES_VERSION &&
            hw->context_type != RETRO_HW_CONTEXT_OPENGLES3 &&
            hw->context_type != RETRO_HW_CONTEXT_OPENGLES2)
        {
            return false;
        }

        isOpenGL = true;
        GLContextMajor = hw->version_major;
        GLContextMinor = hw->version_minor;
        retro_context_reset = hw->context_reset;

        hw->get_current_framebuffer = core_video_get_current_framebuffer;
        hw->get_proc_address = (retro_hw_get_proc_address_t)get_proc_address;
        logger.log(Logger::DEB, "HWRENDER: context_type=%d, major=%d, minor=%d\n",
                   hw->context_type, GLContextMajor, GLContextMinor);

        return true;
    }

    case RETRO_ENVIRONMENT_SET_VARIABLES:
    {
        retro_variable *var = (retro_variable *)data;
        while (var->key != NULL)
        {
            std::string key = var->key;

            const char *start = strchr(var->value, ';');
            start += 2;

            std::string value;
            while (*start != '|' && *start != 0)
            {
                value += *start;
                ++start;
            }

            variables[key] = value;
            logger.log(Logger::DEB, "-> SET_VAR: %s=%s\n", key.c_str(), value.c_str());
            ++var;
        }

        break;
    }

    case RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE:
    {
        float *var = (float *)data;
        *var = 60;
        logger.log(Logger::DEB, "SETTING REFRESH RATE TO 60");
        return true;
    }

    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
    {
        bool *bval = (bool *)data;
        *bval = false;
        return true;
    }

    case RETRO_ENVIRONMENT_SET_ROTATION:
    {
        logger.log(Logger::DEB, "RETRO_ENVIRONMENT_SET_ROTATION not implemented");
        return false;
    }
    case RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION:
    {
        logger.log(Logger::DEB, "RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION not implemented");
        return false;
    }

    case RETRO_ENVIRONMENT_GET_PERF_INTERFACE:
    {
        logger.log(Logger::DEB, "RETRO_ENVIRONMENT_GET_PERF_INTERFACE not implemented");
        return false;
    }

    case RETRO_ENVIRONMENT_GET_LANGUAGE:
    {
        logger.log(Logger::DEB, "RETRO_ENVIRONMENT_GET_LANGUAGE not implemented");
        return false;
    }

    case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
    {
        logger.log(Logger::DEB, "RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK not implemented");
        return false;
    }
    case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
    {
        logger.log(Logger::DEB, "RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE not implemented");
        return false;
    }

    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    {

        const struct retro_controller_info *arg = (retro_controller_info *)data;
        logger.log(Logger::INF, "Controllers Available:");
        for (unsigned x = 0; x < arg->num_types; x++)
        {
            const struct retro_controller_description *type = &arg->types[x];
            logger.log(Logger::INF, " -\t%s: %u", type->desc, type->id);
            // Store the ID and description in the map
            controllerMap[type->id] = type->desc;
        }

        return true; // Return true to indicate successful handling
    }

    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
    {
        logger.log(Logger::DEB, "RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL not implemented");
        return false;
    }

    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
    {
        // we comment this logs otherwise we cannot read the logs, there are too many
        // printf("--LIBRETRO-- RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY not implemented \n");
        return false;
    }

    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    {
        logger.log(Logger::DEB, "RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS not implemented");
        return false;
    }

    case RETRO_ENVIRONMENT_GET_VARIABLE:
    {
        retro_variable *var = (retro_variable *)data;
        // printf("GET_VAR: %s\n", var->key);
        // std::cout << "2 ====>mymap.size() is " << conf_map.size() << '\n';
        bool found = false;

        /*for (const auto &kv : conf_map)
        {

            // printf("Try to find this : %s  with: %s\n", var->key, kv.second.c_str());
            if (strcmp(var->key, kv.first.c_str()) == 0)
            {
                printf("key found: %s  value: %s\n", kv.first.c_str(), kv.second.c_str());
                var->value = kv.second.c_str();
                found = true;
                return true;
            }
        }*/

        std::map<std::string, std::string>::iterator it = conf_map.find(var->key);
        if (it != conf_map.end())
        {
            logger.log(Logger::DEB, "key found: %s  value: %s", it->first.c_str(), it->second.c_str());

            if (it->first.compare("flycast_internal_resolution") == 0 || it->first.compare("flycast2021_internal_resolution") == 0 || it->first.compare("parallel-n64-screensize") == 0)
            {
                if (it->second.compare("320x240") == 0)
                {
                    resolution = R_320_240;
                }
                else if (it->second.compare("640x480") == 0)
                {
                    resolution = R_640_480;
                }
            }

            var->value = it->second.c_str();
            found = true;
            return true;
        }
        if (!found)
        {
            // printf("key not found: settign to default...\n");
            varmap_t::iterator iter = variables.find(var->key);
            if (iter != variables.end())
            {
                var->value = iter->second.c_str();
                logger.log(Logger::DEB, "ENV_VAR (default): %s=%s", var->key, var->value);

                return true;
            }
        }

        return false;
    }

        /*case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE: {
            int audioVideoEnable = 0; // Inizializziamo il flag

            // Impostiamo l'abilitazione del video (bit 0)
            audioVideoEnable |= ENABLE_VIDEO;

            // Impostiamo l'abilitazione/diabilitazione dell'audio (bit 1)
            if (!audio_disabled) {
                audioVideoEnable |= ENABLE_AUDIO;
            }

            // Impostiamo l'abilitazione del fast savestates (bit 2)
            audioVideoEnable |= USE_FAST_SAVESTATES;

            // Impostiamo la disabilitazione dell'audio (bit 3)
            if (audio_disabled) {
                audioVideoEnable |= HARD_DISABLE_AUDIO;
            }
                    *(int*)data = audioVideoEnable;
                }
        */
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
    {
        unsigned int *options_version = (unsigned int *)data;
        *options_version = 1;
        return true;
    }

    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
    {
        const struct retro_core_option_definition *options = ((const struct retro_core_option_definition *)data);
        int i = 0;
        while (options[i].key != nullptr && options[i].default_value != nullptr)
        {
            std::string key = options[i].key;
            std::string value = options[i].default_value;

            variables[key] = value;
            logger.log(Logger::INF, "OPTION: key=%s, value=%s", key.c_str(), value.c_str());
            ++i;
        }

        return true;
    }

    default:
        logger.log(Logger::DEB, "Unhandled env #%u", cmd);

        return false;
    }

    return true;
}

static void core_load(const char *sofile)
{
    void (*set_environment)(retro_environment_t) = NULL;
    void (*set_video_refresh)(retro_video_refresh_t) = NULL;
    void (*set_input_poll)(retro_input_poll_t) = NULL;
    void (*set_input_state)(retro_input_state_t) = NULL;
    void (*set_audio_sample)(retro_audio_sample_t) = NULL;
    void (*set_audio_sample_batch)(retro_audio_sample_batch_t) = NULL;

    memset(&g_retro, 0, sizeof(g_retro));
    g_retro.handle = dlopen(sofile, RTLD_LAZY);

    if (!g_retro.handle)
    {
        logger.log(Logger::ERR, "Failed to load core: %s", dlerror());
        throw std::exception();
    }

    dlerror();

    load_retro_sym(retro_init);
    load_retro_sym(retro_deinit);
    load_retro_sym(retro_api_version);
    load_retro_sym(retro_get_system_info);
    load_retro_sym(retro_get_system_av_info);
    load_retro_sym(retro_set_controller_port_device);
    load_retro_sym(retro_reset);
    load_retro_sym(retro_run);
    load_retro_sym(retro_load_game);
    load_retro_sym(retro_unload_game);
    load_retro_sym(retro_serialize_size);
    load_retro_sym(retro_serialize);
    load_retro_sym(retro_unserialize);
    load_retro_sym(retro_get_memory_data);
    load_retro_sym(retro_get_memory_size);

    load_sym(set_environment, retro_set_environment);
    load_sym(set_video_refresh, retro_set_video_refresh);
    load_sym(set_input_poll, retro_set_input_poll);
    load_sym(set_input_state, retro_set_input_state);
    load_sym(set_audio_sample, retro_set_audio_sample);
    load_sym(set_audio_sample_batch, retro_set_audio_sample_batch);

    set_environment(core_environment);
    set_video_refresh(core_video_refresh);
    set_input_poll(core_input_poll);
    set_input_state(core_input_state);
    set_audio_sample(core_audio_sample);
    set_audio_sample_batch(core_audio_sample_batch);

    /*
    retro_hw_render_callback hw_render_callback;

    // Set fields of retro_hw_render_callback
    hw_render_callback.context_type = RETRO_HW_CONTEXT_OPENGLES2; // Set the context type
    hw_render_callback.version_major = 0; // Set the major version
    hw_render_callback.version_minor = 0; // Set the minor version

    // Other fields can be set similarly

    // Example of setting a callback function
    hw_render_callback.get_current_framebuffer = core_video_get_current_framebuffer;
    hw_render_callback.get_proc_address = (retro_hw_get_proc_address_t)get_proc_address;



    // You can print the values to verify they are set correctly
    printf("context_type: %d, version_major: %d, version_minor: %d\n", hw_render_callback.context_type, hw_render_callback.version_major, hw_render_callback.version_minor);

    // Pass the modified hw_render_callback object to Libretro
    // This could be done during initialization or when setting up hardware rendering
    // For example:

    if (set_environment != NULL) {
        // Chiamata alla funzione retro_set_environment tramite il puntatore a funzione
       set_environment( (retro_environment_t)&hw_render_callback);
    } else {
        // Caricamento fallito, gestire l'errore di conseguenza
        printf("Errore: impossibile caricare la funzione retro_set_environment\n");
    }
           isOpenGL = true;
            GLContextMajor =  0;
            GLContextMinor = 0;
    */
    g_retro.retro_init();
    g_retro.initialized = true;
    logger.log(Logger::INF, "Core loaded.");

    // we postpone this call later because some emulators dont like it (dosbox-core)
    // g_retro.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);

    struct retro_system_info system = {
        0, 0, 0, false, false};

    g_retro.retro_get_system_info(&system);

    logger.log(Logger::DEB, "Core Info: library_name='%s'", system.library_name);
    logger.log(Logger::DEB, "Core Info: library_version='%s'", system.library_version);
    logger.log(Logger::DEB, "Core Info: can extract zip files='%s'", system.block_extract ? "true" : "false");

    // block_extract

    if (strcmp(system.library_name, "Atari800") == 0)
    {
        Retrorun_Core = RETRORUN_CORE_ATARI800;
        g_retro.retro_set_controller_port_device(0, RETRO_DEVICE_ATARI_JOYSTICK);
    }
    else if (strcmp(system.library_name, "ParaLLEl N64") == 0 || strcmp(system.library_name, "ParaLLEl N64 GLN64") == 0)
    {
        Retrorun_Core = RETRORUN_CORE_PARALLEL_N64;
    }
    else if (strcmp(system.library_name, "Flycast") == 0)
    {
        Retrorun_Core = RETRORUN_CORE_FLYCAST;
    }
    coreName = system.library_name;
    Logger::setCoreName(coreName);
    coreVersion = system.library_version;
    coreReadZippedFiles = system.block_extract;
}

static void core_load_game(const char *filename)
{
    struct retro_system_timing timing = {
        60.0f, 10000.0f};
    struct retro_game_geometry geom = {
        100, 100, 100, 100, 1.0f};

    struct retro_system_av_info av = {
        geom, timing};
    struct retro_system_info system = {
        0, 0, 0, false, false};
    struct retro_game_info info = {
        filename,
        0,
        0,
        NULL};
    FILE *file = fopen(filename, "rb");

    if (!file)
        goto libc_error;

    fseek(file, 0, SEEK_END);
    info.size = ftell(file);
    rewind(file);

    g_retro.retro_get_system_info(&system);

    if (!system.need_fullpath)
    {
        info.data = malloc(info.size);

        if (!info.data || !fread((void *)info.data, info.size, 1, file))
            goto libc_error;
    }

    if (!g_retro.retro_load_game(&info))
    {
        logger.log(Logger::ERR, "The core failed to load the content.");
        abort();
    }

    g_retro.retro_get_system_av_info(&av);

    video_configure(&av.geometry);
    audio_init(av.timing.sample_rate);

    return;

libc_error:
    logger.log(Logger::ERR, "Failed to load content '%s'", filename);
    abort();
}

void *unload(void *arg)
{

    if (g_retro.initialized)
    {
        g_retro.retro_deinit();
    }

    if (g_retro.handle)
    {
        dlclose(g_retro.handle);
        exitFlag = 0;
    }
    throw std::runtime_error("Force exiting retrorun.\n");
}
/*
void unload(void)
{

    if (g_retro.initialized)
    {
        g_retro.retro_deinit();
    }

    if (g_retro.handle)
    {
        dlclose(g_retro.handle);
        exitFlag = 0;
    }
    throw std::runtime_error("Force exiting retrorun.\n");
}
*/
static const char *FileNameFromPath(const char *fullpath)
{
    // Find last slash
    const char *ptr = strrchr(fullpath, '/');
    if (!ptr)
    {
        ptr = fullpath;
    }
    else
    {
        ++ptr;
    }

    return ptr;
}

/*static char *PathCombine(const char *path, const char *filename)
{
    int len = strlen(path);
    int total_len = len + strlen(filename);

    char *result = NULL;

    if (path[len - 1] != '/')
    {
        ++total_len;
        result = (char *)calloc(total_len + 1, 1);
        strcpy(result, path);
        strcat(result, "/");
        strcat(result, filename);
    }
    else
    {
        result = (char *)calloc(total_len + 1, 1);
        strcpy(result, path);
        strcat(result, filename);
    }

    return result;
}*/

inline int getRetroMemory()
{

    return /*isFlycast() ? RETRO_MEMORY_VIDEO_RAM :*/ RETRO_MEMORY_SAVE_RAM;
}

static int LoadState(const char *saveName)
{
    FILE *file = fopen(saveName, "rb");
    if (!file)
    {
        logger.log(Logger::ERR, "Error loading state: File '%s' not found!", saveName);
        return -1;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    if (size < 1)
    {
        logger.log(Logger::ERR, "Error loading state, in file '%s': size is wrong!", saveName);
        return -1;
    }
    void *ptr = malloc(size);
    if (!ptr)
    {
        logger.log(Logger::ERR, "Error loading state, ptr not valid aborting...");
        abort();
    }
    size_t count = fread(ptr, 1, size, file);

    if ((size_t)size != count)
    {
        logger.log(Logger::ERR, "Error loading state, in file '%s': size mismatch!", saveName);
        free(ptr);
        abort();
    }
    fclose(file);
    bool result = g_retro.retro_unserialize(ptr, size);
    free(ptr);
    if (result)
    {
        logger.log(Logger::INF, "File '%s': loaded correctly!", saveName);
    }
    else
    {
        logger.log(Logger::WARN, "File '%s': loaded correctly but with no effects!", saveName);
    }
    return 0;
}

static int LoadSram(const char *saveName)
{
    try
    {

        FILE *file = fopen(saveName, "rb");
        if (!file)
        {
            logger.log(Logger::ERR, "Error loading sram: File '%s' not found!", saveName);
            return -1;
        }

        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        rewind(file);

        size_t sramSize = g_retro.retro_get_memory_size(getRetroMemory());
        if (size < 1)
        {
            logger.log(Logger::ERR, "Error loading sram, memory size wrong!");
            return -1;
        }

        if (size != (long)sramSize)
        {
            logger.log(Logger::ERR, "Error loading sram, in file '%s': size mismatch!", saveName);
            return -1;
        }
        void *ptr = g_retro.retro_get_memory_data(getRetroMemory());
        if (!ptr)
        {
            logger.log(Logger::ERR, "Error loading sram, file '%s': contains wrong memory data!", saveName);
            abort();
        }
        size_t count = fread(ptr, 1, size, file);
        if ((size_t)size != count)
        {
            logger.log(Logger::ERR, "Error loading sram, in file '%s': size mismatch!", saveName);
            abort();
        }
        fclose(file);
        logger.log(Logger::INF, "File '%s': loaded correctly!\n", saveName);
    }
    catch (const std::exception &e) // caught by reference to base
    {
        logger.log(Logger::ERR, "a standard exception was caught, with message: '%s'", e.what());
    }
    return 0;
}

static void SaveState(const char *saveName)
{
    size_t size = g_retro.retro_serialize_size();
    void *ptr = malloc(size);
    if (!ptr)
    {
        logger.log(Logger::ERR, "Error saving state: ptr not valid!");
        abort();
    }
    g_retro.retro_serialize(ptr, size);
    FILE *file = fopen(saveName, "wb");
    if (!file)
    {
        logger.log(Logger::ERR, "Error saving state: File '%s' cannot be opened!", saveName);
        free(ptr);
        abort();
    }
    size_t count = fwrite(ptr, 1, size, file);
    if (count != size)
    {
        logger.log(Logger::ERR, "Error saving state: File '%s' count not valid!", saveName);
        free(ptr);
        abort();
    }
    fclose(file);
    free(ptr);
    logger.log(Logger::INF, "File '%s': saved correctly!", saveName);

    return;
}

static void SaveSram(const char *saveName)
{
    size_t size = g_retro.retro_get_memory_size(getRetroMemory());
    if (size < 1)
    {
        logger.log(Logger::ERR, "nothing to save in srm file!, %zu", size);
        return;
    }
    void *ptr = g_retro.retro_get_memory_data(getRetroMemory());
    if (!ptr)
    {
        logger.log(Logger::ERR, "Error saving sram: ptr not valid!");
        abort();
    }

    FILE *file = fopen(saveName, "wb");
    if (!file)
    {
        logger.log(Logger::ERR, "Error saving sram: File '%s' cannot be opened!", saveName);
        abort();
    }

    size_t count = fwrite(ptr, 1, size, file);
    if (count != size)
    {
        logger.log(Logger::ERR, "Error saving sram: File '%s' count not valid!", saveName);
        abort();
    }

    fclose(file);
    logger.log(Logger::INF, "Sram saved!");
}

std::string getSystemFromRomPath(const char *fullpath)
{
    std::string arg_rom_string(fullpath);
    size_t slash = arg_rom_string.find_last_of("\\/");
    std::string dirPath = (slash != std::string::npos) ? arg_rom_string.substr(0, slash) : arg_rom_string;
    size_t slash2 = dirPath.find_last_of("\\/");
    std::string system = (slash2 != std::string::npos) ? dirPath.substr(slash2 + 1, dirPath.length()) : dirPath;
    logger.log(Logger::INF, "system='%s'", system.c_str());
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
        return 0.0f; // will be the default (provided by core)
}

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

int getClosestValue(float value)
{
    int result = 1; // 4:3 by default
    // we need to refresh the game_aspect_ratio
    // float current_game_aspect_ratio = game_aspect_ratio == 0.0f ? aspect_ratio : game_aspect_ratio;
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
    // float closestAspectRatio = 0.0f;

    for (const auto &pair : aspectRatioMap)
    {
        float difference = std::abs(pair.first - value);
        if (difference < minDifference)
        {
            minDifference = difference;
            // closestAspectRatio = pair.first;
            result = pair.second;
        }
    }
    return result;
}

int getAspectRatioSettings()
{

    int result = getClosestValue(aspect_ratio);

    return result;
}

auto setAspectRatioSettings = [](int button) -> std::function<void(int)>
{
    const int aspect_ratio_count = sizeof(aspect_ratio_names_array) / sizeof(aspect_ratio_names_array[0]);

    int currentIndex = getClosestValue(aspect_ratio);

    if (button == RIGHT)
    {
        currentIndex = currentIndex + 1;
    }
    else if (button == LEFT)
    {
        currentIndex = currentIndex - 1;
    }

    if (currentIndex < 0)
    {
        currentIndex = aspect_ratio_count - 1;
    }
    else if (currentIndex >= aspect_ratio_count - 1)
    {
        currentIndex = 0;
    }
    aspect_ratio = getAspectRatio(aspect_ratio_names_array[currentIndex]);
    prepareScreen(currentWidth, currentHeight);
    return std::function<void(int)>();
};

TateState getTateMode(const std::string tate)
{
    if (tate == "enabled")
        return ENABLED;
    else if (tate == "disabled")
        return DISABLED;
    else if (tate == "reversed")
        return REVERSED;
    else if (tate == "auto")
        return AUTO;
    else
        return DISABLED; // will be the default
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
        return Logger::INF; // will be the default
}

bool fileExists(const char *path)
{
    if (access(path, F_OK) != -1)
    {
        // Il file esiste
        return true;
    }
    else
    {
        // Il file non esiste
        return false;
    }
}

// Funzione per ottenere la data e l'orario dell'ultima modifica di un file
std::string getLastModifiedTime(const char *path)
{
    struct stat fileInfo;
    if (stat(path, &fileInfo) != 0)
    {
        // Errore nell'ottenere le informazioni sul file
        return "Errore nell'ottenere le informazioni sul file";
    }

    // Ottieni l'orario dell'ultima modifica del file
    std::time_t modifiedTime = fileInfo.st_mtime;

    // Converti l'orario in una struttura tm (tempo locale)
    struct tm *timeinfo = std::localtime(&modifiedTime);

    // Formatta la data e l'orario come stringa
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

    return buffer;
}

char *createSramPath(const std::string &arg_rom, const std::string &opt_savedir)
{
    const char *fileName = FileNameFromPath(arg_rom.c_str());

    std::string fullNameString(fileName);
    // removing extension
    size_t lastindex = fullNameString.find_last_of(".");
    std::string rawname = fullNameString.substr(0, lastindex);

    std::string srmAutoPath = opt_savedir + "/<gameName>.srm";
    std::string srmPathFinal = replace(srmAutoPath, "<gameName>", rawname);

    char *sramPath = (char *)malloc(srmPathFinal.length() + 1);
    strcpy(sramPath, srmPathFinal.c_str());

    // logger.log(Logger::INF, "sramPath='%s'", sramPath);
    return sramPath;
}

char *createSavePath(const std::string &arg_rom, const std::string &opt_savedir)
{
    const char *fileName = FileNameFromPath(arg_rom.c_str());
    std::string fullNameString(fileName);
    // removing extension
    size_t lastindex = fullNameString.find_last_of(".");
    std::string rawname = fullNameString.substr(0, lastindex);

    std::string stateAutoPath = opt_savedir + "/<gameName>.rrstate.auto";
    std::string statePathFinal = replace(stateAutoPath, "<gameName>", rawname);

    char *savePath = (char *)malloc(statePathFinal.length() + 1);
    strcpy(savePath, statePathFinal.c_str());

    // logger.log(Logger::INF, "savePath='%s'", savePath);
    return savePath;
}

void initConfig()
{

    std::string config_file = "retrorun.cfg";
    

    // Check if the local config file exists
    if (fileExists(config_file.c_str()))
    {
        logger.log(Logger::INF, "Using local configuration file: '%s'", config_file.c_str());
    }
    else
    {
        logger.log(Logger::INF, "Local configuration file not found. Using default configuration file: '%s'", opt_setting_file);
        config_file = opt_setting_file; // Use the default config file
    }
    std::ifstream infile(config_file);
    if (!infile.good())
    {
        logger.log(Logger::ERR, "Configuration file:'%s' doesn't exist default core settings will be used", opt_setting_file);
    }
    else
    {
        logger.log(Logger::INF, "Reading configuration file:'%s'", config_file.c_str());
        initMapConfig(config_file.c_str());
        try
        {
            const std::string &ssFolderValue = conf_map.at("retrorun_screenshot_folder");
            screenShotFolder = ssFolderValue;
            logger.log(Logger::INF, "Screenshot folder:%s", screenShotFolder.c_str());
        }
        catch (...)
        {
            logger.log(Logger::WARN, "retrorun_screenshot_folder parameter not found in retrorun.cfg using default folder (/storage/roms/screenshots).");
            screenShotFolder = "/storage/roms/screenshots";
        }

        try
        {
            const std::string &ssFps_counter = conf_map.at("retrorun_fps_counter");
            input_fps_requested = ssFps_counter == "enabled" ? true : false;
            logger.log(Logger::INF, "retrorun_fps_counter :%s", input_fps_requested ? "TRUE" : "FALSE");
        }
        catch (...)
        {
            logger.log(Logger::WARN, "retrorun_fps_counter parameter not found in retrorun.cfg using defaulf value (disabled).");
            input_fps_requested = false;
        }

        if (opt_aspect != 0.0f)
        {
            logger.log(Logger::INF, "aspect_ratio forced from command line.");
        }
        else
        {
            try
            {
                const std::string &arValue = conf_map.at("retrorun_aspect_ratio");
                opt_aspect = getAspectRatio(arValue);
                logger.log(Logger::INF, "retrorun_aspect_ratio :%f", opt_aspect);
            }
            catch (...)
            {
                logger.log(Logger::WARN, "retrorun_aspect_ratio parameter not found in retrorun.cfg using default value (core provided).");
            }
        }
        try
        {
            const std::string &asValue = conf_map.at("retrorun_auto_save");
            auto_save = asValue == "true" ? true : false;
            logger.log(Logger::INF, "retrorun_auto_save: %s.", auto_save ? "true" : "false");
            if (isFlycast2021())
            {
                auto_save = false;
                logger.log(Logger::WARN, "retrorun_auto_save disabled on Flycast2021, because it doesnt work!");
            }
        }
        catch (...)
        {
            logger.log(Logger::WARN, "retrorun_auto_save parameter not found in retrorun.cfg using default value (%s).", auto_save ? "true" : "false");
        }

        try
        {
            const std::string &asValue = conf_map.at("retrorun_auto_load");
            auto_load = asValue == "true" ? true : false;
            logger.log(Logger::INF, "retrorun_auto_load: %s.", auto_load ? "true" : "false");
        }
        catch (...)
        {
            logger.log(Logger::INF, "By defualt retrorun auto_load will be the same as auto_save.");
            auto_load = auto_save;
            logger.log(Logger::WARN, "retrorun_auto_load parameter not found in retrorun.cfg using default value (%s).", auto_load ? "true" : "false");
        }

       /* try
        {
            const std::string &lasValue = conf_map.at("retrorun_force_left_analog_stick");

            force_left_analog_stick = lasValue == "true" ? true : false;
            logger.log(Logger::INF, "retrorun_force_left_analog_stick: %s.", force_left_analog_stick ? "true" : "false");
        }
        catch (...)
        {
            logger.log(Logger::WARN, "etrorun_force_left_analog_stick parameter not found in retrorun.cfg using default value (%s).", force_left_analog_stick ? "true" : "false");
        }
*/
        try
        {
            const std::string &tflValue = conf_map.at("retrorun_loop_declared_fps");

            runLoopAtDeclaredfps = tflValue == "false" ? false : true;
            logger.log(Logger::INF, "retrorun_loop_declared_fps: %s.", runLoopAtDeclaredfps ? "true" : "false");
        }
        catch (...)
        {
            logger.log(Logger::WARN, "retrorun_loop_declared_fps parameter not found in retrorun.cfg using default value (%s).", runLoopAtDeclaredfps ? "true" : "false");
        }

        try
        {
            const std::string &asValue = conf_map.at("retrorun_swap_l1r1_with_l2r2");
            swapL1R1WithL2R2 = asValue == "true" ? true : false;
            logger.log(Logger::INF, "retrorun_swap_l1r1_with_l2r2: %s.", swapL1R1WithL2R2 ? "true" : "false");
        }
        catch (...)
        {
            logger.log(Logger::WARN, "retrorun_swap_l1r1_with_l2r2 parameter not found in retrorun.cfg using default value (%s).", swapL1R1WithL2R2 ? "true" : "false");
        }
        try
        {
            const std::string &asValue = conf_map.at("retrorun_swap_sticks");
            swapSticks = asValue == "true" ? true : false;
            logger.log(Logger::INF, "retrorun_swap_sticks: %s.", swapSticks ? "true" : "false");
        }
        catch (...)
        {
            logger.log(Logger::INF, "retrorun_swap_sticks parameter not found in retrorun.cfg using default value (%s).", swapSticks ? "true" : "false");
        }

        try
        {
            const std::string &audioBufferValue = conf_map.at("retrorun_audio_buffer");
            if (!audioBufferValue.empty())
            {
                retrorun_audio_buffer = stoi(audioBufferValue);
                new_retrorun_audio_buffer = retrorun_audio_buffer;
                logger.log(Logger::INF, "retrorun_audio_buffer: %d.", retrorun_audio_buffer);
            }
        }
        catch (...)
        {
            logger.log(Logger::WARN, "retrorun_audio_buffer parameter not found in retrorun.cfg using default value (-1).");
        }

        try
        {
            const std::string &mouseSpeedValue = conf_map.at("retrorun_mouse_speed_factor");
            if (!mouseSpeedValue.empty())
            {
                retrorun_mouse_speed_factor = stoi(mouseSpeedValue);
                logger.log(Logger::INF, "retrorun_mouse_speed_factor: %d.", retrorun_mouse_speed_factor);
            }
        }
        catch (...)
        {
            logger.log(Logger::WARN, "retrorun_mouse_speed_factor parameter not found in retrorun.cfg using default value (5).");
        }

        try
        {
            const std::string &arValue = conf_map.at("retrorun_tate_mode");
            tateState = getTateMode(arValue);
            /** TODO: fix this it should not return the opt_aspect*/
            logger.log(Logger::INF, "retrorun_tate_mode :%f\n", opt_aspect);
        }
        catch (...)
        {
            logger.log(Logger::WARN, "retrorun_tate_mode parameter not found in retrorun.cfg using default value (DISABLED).\n");
        }

        try
        {
            const std::string &arValue = conf_map.at("retrorun_log_level");
            logger.setLogLevel(getLogLevel(arValue));

            logger.log(Logger::INF, "retrorun_log_level :%s\n", arValue);
        }
        catch (...)
        {
            logger.log(Logger::WARN, "retrorun_log_level parameter not found in retrorun.cfg using default value (INFO).\n");
        }

        try
        {
            const std::string &asValue = conf_map.at("retrorun_device_name");
            retrorun_device_name = asValue; 
            logger.log(Logger::INF, "retrorun_device_name: %s.", retrorun_device_name.c_str());
            
        }
        catch (...)
        {
            logger.log(Logger::WARN, "retrorun_device_name parameter not found in retrorun.cfg, device name will be detected in a different way..." );
        }


        processVideoInAnotherThread = (isRG552() /*|| isRG503()*/) ? true : false;

        adaptiveFps = false;
        logger.log(Logger::INF, "Configuration initialized.");
    }

    infile.close();
}

void fake(int button)
{
    // printf("fake function...");
}

int getTateMode()
{

    return (int)tateState;
}

auto setTateMode = [](int button) -> std::function<void(int)>
{
    if (button == RIGHT)
    {
        tateState = static_cast<TateState>((tateState + 1) % (AUTO + 1));
    }
    else if (button == LEFT)
    {
        tateState = static_cast<TateState>((tateState - 1) < DISABLED ? AUTO : (tateState - 1));
    }
    return std::function<void(int)>();
};

int getSwapTriggers()
{
    return swapL1R1WithL2R2 ? 1 : 0;
}

auto setSwapTriggers = [](int button) -> std::function<void(int)>
{
    if (button == LEFT || button == RIGHT)
    {
        swapL1R1WithL2R2 = !swapL1R1WithL2R2;
    }
    return std::function<void(int)>();
};

int getSwapSticks()
{
    return swapSticks ? 1 : 0;
}

auto setSwapSticks = [](int button) -> std::function<void(int)>
{
    if (button == LEFT || button == RIGHT)
    {
        swapSticks = !swapSticks;
    }
    return std::function<void(int)>();
};

int getLockDeclaredFPS()
{
    return runLoopAtDeclaredfps ? 1 : 0;
}

auto setLockDeclaredFPS = [](int button) -> std::function<void(int)>
{
    if (button == LEFT || button == RIGHT)
    {
        runLoopAtDeclaredfps = !runLoopAtDeclaredfps;
    }
    return std::function<void(int)>();
};

int getAudioDisabled()
{
    return audio_disabled ? 1 : 0;
}

auto setAudioDisabled = [](int button) -> std::function<void(int)>
{
    if (button == LEFT || button == RIGHT)
    {
        audio_disabled = !audio_disabled;
    }
    return std::function<void(int)>();
};

int audio_buffer_array[] = {
    -1,
    1,
    256,
    512,
    1024,
    2048,
    4096};

int getAudioBuffer()
{
    return new_retrorun_audio_buffer;
}

auto setAudioBuffer = [](int button) -> std::function<void(int)>
{
    int audio_buffer_array_size = sizeof(audio_buffer_array) / sizeof(audio_buffer_array[0]);

    int current_index = -1; // indice corrente di retrorun_audio_buffer nell'array
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
    {

        new_index = (current_index + 1) % audio_buffer_array_size; // Incrementiamo l'indice in modo circolare
    }
    else if (button == LEFT)
    {

        new_index = (current_index - 1 + audio_buffer_array_size) % audio_buffer_array_size; // Decrementiamo l'indice in modo circolare
    }
    new_retrorun_audio_buffer = audio_buffer_array[new_index];
    return std::function<void(int)>();
};

int getBrightnessValue()
{
    int value = brightnessState.level;
    return value;
}
int step_left_right = 10;

auto setBrightnessValue = [](int button) -> std::function<void(int)>
{
    int selectedBrigthness = brightnessState.level;
    if (button == LEFT)
    {
        selectedBrigthness -= step_left_right;
        if (selectedBrigthness < 1)
            selectedBrigthness = step_left_right; // preventiing black screenn
        if (selectedBrigthness > 100)
            selectedBrigthness = 100;
        go2_input_brightness_write(selectedBrigthness);
    }
    else if (button == RIGHT)
    {
        selectedBrigthness += step_left_right;
        if (selectedBrigthness < 1)
            selectedBrigthness = step_left_right; // preventiing black screenn
        if (selectedBrigthness > 100)
            selectedBrigthness = 100;
        go2_input_brightness_write(selectedBrigthness);
    }
    return std::function<void(int)>();
};

int getAudioValue()
{
    int value = getVolume();
    return value;
}

auto setAudioValue = [](int button) -> std::function<void(int)>
{
    int selectedVolume = getVolume();
    if (button == LEFT)
    {
        selectedVolume -= step_left_right;
        if (selectedVolume < 0)
            selectedVolume = 0;
        if (selectedVolume > 100)
            selectedVolume = 100;
        setVolume(selectedVolume);
    }
    else if (button == RIGHT)
    {
        selectedVolume += step_left_right;
        if (selectedVolume < 0)
            selectedVolume = 0;
        if (selectedVolume > 100)
            selectedVolume = 100;
        setVolume(selectedVolume);
    }
    return std::function<void(int)>();
};



int getDeviceType()
{
    return deviceTypeSelected;
}

auto setDeviceType = [](int button) -> std::function<void(int)>
{
    auto it = controllerMap.find(deviceTypeSelected);
    if (it != controllerMap.end())
    {
        if (button == LEFT)
        {
            if (it == controllerMap.begin()) // Se siamo già al primo elemento, torna all'ultimo
            {
                it = controllerMap.end();
            }
            deviceTypeSelected = (--it)->first;
        }
        else if (button == RIGHT)
        {
            if (++it == controllerMap.end()) // Se siamo già all'ultimo elemento, torna al primo
            {
                it = controllerMap.begin();
            }
            deviceTypeSelected = it->first;
        }
    }
    g_retro.retro_set_controller_port_device(0, deviceTypeSelected);
    //return setDeviceType; // Ritorna se stesso per poter essere riutilizzato
    return std::function<void(int)>();
};

int getAnalogToDigital()
{
    return analogToDigital;
}

auto setAnalogToDigital = [](int button) -> std::function<void(int)>
{
    if (button == LEFT)
        {
            if (analogToDigital == NONE)
                analogToDigital = RIGHT_ANALOG_FORCED;
            else
                analogToDigital = static_cast<AnalogToDigital>(static_cast<int>(analogToDigital) - 1);
        }
        else if (button == RIGHT)
        {
            if (analogToDigital == RIGHT_ANALOG_FORCED)
                analogToDigital = NONE;
            else
                analogToDigital = static_cast<AnalogToDigital>(static_cast<int>(analogToDigital) + 1);
        }
    return std::function<void(int)>();
};


void resume(int button)
{
    if (button == A_BUTTON)
    {
        input_info_requested = false;
    }
}

auto getSlotName = [](int slotNumber, std::string type) -> std::string
{
    std::string result = "<empty>";
    char *savePath = createSavePath(arg_rom, opt_savedir);
    std::string savePath1 = savePath;
    savePath1 += slotNumber == 1 ? "" : "" + std::to_string(slotNumber - 1);
    if (fileExists(savePath1.c_str()))
    {
        result = getLastModifiedTime(savePath1.c_str());
    }
    std::free(savePath);
    // free(savePath);
    if (type == "Load")
    {
        return "<- " + std::to_string(slotNumber) + " " + result;
    }
    else
    {
        return "-> " + std::to_string(slotNumber) + " " + result;
    }
};

auto loadSaveSlot = [](int slotNumber, std::string type) -> std::function<void(int)>
{
    return [slotNumber, type](int button)
    {
        if (button == A_BUTTON)
        {
            /*if (getSlotName(slotNumber, type).find("empty") != std::string::npos)
            {
                return;
            }*/
            logger.log(Logger::INF, "Slot number :%d\n", slotNumber);
            char *savePath = createSavePath(arg_rom, opt_savedir);
            std::string savePath1 = savePath;
            savePath1 += slotNumber == 1 ? "" : "" + std::to_string(slotNumber - 1);

            if (type == "Load")
            {
                logger.log(Logger::INF, "loading file :%s\n", savePath1);
                LoadState(savePath1.c_str());
            }
            else
            {
                logger.log(Logger::INF, "saving file :%s\n", savePath1.c_str());
                SaveState(savePath1.c_str());
            }
            free(savePath);
            sleep(1);
        }
    };
};

// Definisci una funzione statica che chiama la tua lambda function con i parametri richiesti
static void loadSaveSlotWrapper(int button, int slotNumber, std::string type)
{
    auto slotFunction = loadSaveSlot(slotNumber, type);
    slotFunction(button);
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

#include <chrono> // Add this include for chrono functionalities
#include <thread> // Add this include for thread sleep

using namespace std::chrono;

int main(int argc, char *argv[])
{
    printf("\n");
    logger.log(Logger::INF, "#### RETRORUN %s ####", release.c_str());
    printf("\n");
    initConfig();
    getDeviceName(); // we need this call here (otherwise it doesnt work because the methos is called only later , this need to be refactored)
    

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
            //force_left_analog_stick = false;
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

    // gpio_joypad normally is false and should be set to true only for MP and 552
    // but the parameter sesetnd via command line wins so if that is true we leave it true
    if (!gpio_joypad)
    {
        if (isRG351MP() || isRG552())
        {
            gpio_joypad = true;
        }
    }

    int remaining_args = argc - optind;
    int remaining_index = optind;
    logger.log(Logger::INF, "remaining_args=%d", remaining_args);

    if (remaining_args < 2)
    {
        logger.log(Logger::ERR, "Usage: %s [-s savedir] [-d systemdir] [-a aspect] core rom", argv[0]);
        exit(EXIT_FAILURE);
    }

    // return 0;
    if (optind < argc)
    {
        logger.log(Logger::INF, "non-option ARGV-elements:");
        while (optind < argc)
            logger.log(Logger::INF, " - %s ", argv[optind++]);
    }

    arg_core = argv[remaining_index++];
    arg_rom = argv[remaining_index++];

    input_gamepad_read();

    core_load(arg_core);
    // conf_map.clear();

    // on MP is reverted (4:3 -> 3:4)
    if (isSwanStation() && (isRG351V() || isRG351MP()))
    {
        opt_aspect = 0.75f;
    }

    core_load_game(arg_rom);
    // conf_map.clear();

    go2_input_state_t *gamepadState = input_gampad_current_get();
    if (go2_input_state_button_get(gamepadState, Go2InputButton_F1) == ButtonState_Pressed)
    {
        logger.log(Logger::WARN, "Forcing restart due to button press (F1)...");
        opt_restart = true;
    }

    // State
    // Chiamata alla funzione createSramPath
    char *sramPath = createSramPath(arg_rom, opt_savedir);
    // Chiamata alla funzione createSavePath
    char *savePath = createSavePath(arg_rom, opt_savedir);

    logger.log(Logger::INF, "savePath='%s'", savePath);
    logger.log(Logger::INF, "sramPath='%s'", sramPath);

    if (opt_restart)
    {
        logger.log(Logger::WARN, "Restarting...");
    }
    else
    {

        if (isFlycast2021())
        {
            auto_load = false;
            logger.log(Logger::WARN, "retrorun_auto_load disabled on Flycast2021, because it doesnt work!");
        }
        if (auto_load)
        {
            input_message = true;
            status_message = "Loading saved game...";
            logger.log(Logger::INF, "Loading saved state - File '%s'", savePath);

            if (isParalleln64() || isDosBox())
            {
                // for parallel n64 we need to wait till the core is fully initialized then we call a retro_run to be sure
                // for the other cores is also fine but with pcsx rearmed it has problems
                sleep(1);
                g_retro.retro_run();
            }

            LoadState(savePath);
            sleep(3);
            // input_message = false;
        }
    }

    logger.log(Logger::INF, "Loading sram - File '%s'", sramPath);
    LoadSram(sramPath);
    logger.log(Logger::DEB, "Entering render loop.");

    // const char* batteryStateDesc[] = { "UNK", "DSC", "CHG", "FUL" };

    // struct timeval startTime;
    // struct timeval endTime;
    double elapsed = 0;
    int totalFrames = 0;

    // sleep(1); // some cores (like yabasanshiro) from time to time hangs on retro_run otherwise

    struct retro_system_av_info info;
    g_retro.retro_get_system_av_info(&info);
    logger.log(Logger::DEB, "System Info - aspect_ratio: %f", info.geometry.aspect_ratio);
    logger.log(Logger::DEB, "System Info - base_width: %d", info.geometry.base_width);
    logger.log(Logger::DEB, "System Info - base_height: %d", info.geometry.base_height);
    logger.log(Logger::DEB, "System Info - max_width: %d", info.geometry.max_width);
    logger.log(Logger::DEB, "System Info - max_height: %d", info.geometry.max_height);
    logger.log(Logger::DEB, "System Info - fps: %f", info.timing.fps);
    logger.log(Logger::DEB, "System Info - sample_rate: %f", info.timing.sample_rate);

    auto prevClock = std::chrono::high_resolution_clock::now();
    auto totClock = std::chrono::high_resolution_clock::now();
    double max_fps = info.timing.fps;
    double previous_fps = 0;
    originalFps = info.timing.fps;
    if (max_fps < 1)
    { // just to be sure info are there
        max_fps = 60;
    }
    if (originalFps < 1)
    { // just to be sure info are ther
        originalFps = 60;
    }
    // adaptiveFps = isFlycast() ? true: false;
    bool redrawInfo = true;
    // we postpone this here because if we do it before some emulators dont like it (Dosbox core)
    g_retro.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);

    unsigned long long countNumFps = 0;
    unsigned long long countValFps = 0;

    auto start_time = std::chrono::steady_clock::now(); // loop start time
    bool startCalAvgFps = false;

    // menu

    MenuItem menuItem_slot1Load = MenuItem("Are you sure?", [](int arg)
                                           { loadSaveSlotWrapper(arg, 1, "Load"); });
    menuItem_slot1Load.setQuestionItem();
    std::vector<MenuItem> slot1Load_sure = {menuItem_slot1Load};

    Menu menuInfoSlot1Load = Menu("Slot1", slot1Load_sure);

    MenuItem menuItem_slot2Load = MenuItem("Are you sure?", [](int arg)
                                           { loadSaveSlotWrapper(arg, 2, "Load"); });
    menuItem_slot2Load.setQuestionItem();
    std::vector<MenuItem> slot2Load_sure = {menuItem_slot2Load};

    Menu menuInfoSlot2Load = Menu("Slot2", slot2Load_sure);

    MenuItem menuItem_slot3Load = MenuItem("Are you sure?", [](int arg)
                                           { loadSaveSlotWrapper(arg, 3, "Load"); });
    menuItem_slot3Load.setQuestionItem();
    std::vector<MenuItem> slot3Load_sure = {menuItem_slot3Load};

    Menu menuInfoSlot3Load = Menu("Slot3", slot3Load_sure);

    std::vector<MenuItem> itemsLoadStateLoad = {
        MenuItem([]()
                 { return getSlotName(1, "Load"); },
                 &menuInfoSlot1Load, fake),
        MenuItem([]()
                 { return getSlotName(2, "Load"); },
                 &menuInfoSlot2Load, fake),
        MenuItem([]()
                 { return getSlotName(3, "Load"); },
                 &menuInfoSlot3Load, fake)

    };

    Menu menuLoadState = Menu("Load State", itemsLoadStateLoad);

    // save state

    MenuItem menuItem_slot1Save = MenuItem("Are you sure?", [](int arg)
                                           { loadSaveSlotWrapper(arg, 1, "Save"); });
    menuItem_slot1Save.setQuestionItem();
    std::vector<MenuItem> slot1Save_sure = {menuItem_slot1Save};

    Menu menuInfoSlot1Save = Menu("Slot1", slot1Save_sure);

    MenuItem menuItem_slot2Save = MenuItem("Are you sure?", [](int arg)
                                           { loadSaveSlotWrapper(arg, 2, "Save"); });
    menuItem_slot2Save.setQuestionItem();
    std::vector<MenuItem> slot2Save_sure = {menuItem_slot2Save};

    Menu menuInfoSlot2Save = Menu("Slot2", slot2Save_sure);

    MenuItem menuItem_slot3Save = MenuItem("Are you sure?", [](int arg)
                                           { loadSaveSlotWrapper(arg, 3, "Save"); });
    menuItem_slot3Save.setQuestionItem();
    std::vector<MenuItem> slot3Save_sure = {menuItem_slot3Save};

    Menu menuInfoSlot3Save = Menu("Slot3", slot3Save_sure);

    std::vector<MenuItem> itemsLoadStateSave = {
        MenuItem([]()
                 { return getSlotName(1, "Save"); },
                 &menuInfoSlot1Save, fake),
        MenuItem([]()
                 { return getSlotName(2, "Save"); },
                 &menuInfoSlot2Save, fake),
        MenuItem([]()
                 { return getSlotName(3, "Save"); },
                 &menuInfoSlot3Save, fake)

    };

    Menu menuSaveState = Menu("Load State", itemsLoadStateSave);

    std::vector<MenuItem> itemsState = {
        MenuItem("Load state", &menuLoadState, fake),
        MenuItem("Save state", &menuSaveState, fake),
    };

    Menu menuState = Menu("State", itemsState);

    std::vector<MenuItem> itemsSystem = {
        MenuItem("Volume", getAudioValue, setAudioValue, "%"),
        MenuItem("Brightness", getBrightnessValue, setBrightnessValue, "%"),
    };

    Menu menuSystem = Menu("System", itemsSystem);


MenuItem deviceType("Device type", getDeviceType, setDeviceType, "device-type");
deviceType.setPossibleValues(controllerMap);

    std::vector<MenuItem> itemsControl = {
        deviceType,
        MenuItem("Analog to DPAD", getAnalogToDigital, setAnalogToDigital, "analog-to-digital"),
        MenuItem("Swap triggers", getSwapTriggers, setSwapTriggers, "bool"),
        MenuItem("Swap analog sticks", getSwapSticks, setSwapSticks, "bool"),
    };

    Menu menuControl = Menu("Control", itemsControl);

    std::vector<MenuItem> itemsAudio = {
        MenuItem("Audio Buffer", getAudioBuffer, setAudioBuffer, ""),
        MenuItem("Audio Disabled", getAudioDisabled, setAudioDisabled, "bool"),
    };

    Menu menuAudio = Menu("Audio", itemsAudio);

    std::vector<MenuItem> itemsVideo = {
        MenuItem("Aspect ratio", getAspectRatioSettings, setAspectRatioSettings, "aspect-ratio"),
        MenuItem("Lock FPS", getLockDeclaredFPS, setLockDeclaredFPS, "bool"),
        MenuItem("Tate mode", getTateMode, setTateMode, "rotation"),
    };

    Menu menuVideo = Menu("Video", itemsVideo);

    std::vector<MenuItem> itemsSettings = {
        MenuItem("System", &menuSystem, fake),
        MenuItem("Control", &menuControl, fake),
        MenuItem("Video", &menuVideo, fake),
        MenuItem("Audio", &menuAudio, fake),
    };

    Menu menuSettings = Menu("Settings", itemsSettings);

    // define the menu and menu items for Info
    std::vector<MenuItem> device = {
        MenuItem(SHOW_DEVICE, NULL)};
    Menu menuInfoDevice = Menu("Device", device);

    std::vector<MenuItem> core = {
        MenuItem(SHOW_CORE, NULL)};
    Menu menuInfoCore = Menu("Libretro core", core);

    std::vector<MenuItem> game = {
        MenuItem(SHOW_GAME, NULL)};
    Menu menuInfoGame = Menu("Current game", game);

    std::vector<MenuItem> itemsInfo = {
        MenuItem("Device", &menuInfoDevice, fake),
        MenuItem("Libretro core", &menuInfoCore, fake),
        MenuItem("Current game", &menuInfoGame, fake)};

    // MenuItem menuItem_q = MenuItem("Are you sure?", quit);
    MenuItem menuItem_q = MenuItem("Are you sure?", [](int button)
                                   { if (button == A_BUTTON) { isRunning = false; } });

    menuItem_q.setQuitItem();
    std::vector<MenuItem> quit_sure = {menuItem_q};

    Menu menuInfoQuit = Menu("Quit", quit_sure);

    // define Main Menu
    Menu menuInfo = Menu("Info", itemsInfo);



MenuItem menu_item_restart = MenuItem("Are you sure?", [](int button)
                                           { if (button == A_BUTTON) { g_retro.retro_reset(); }});
    menu_item_restart.setQuestionItem();
    std::vector<MenuItem> menu_restart_sure = {menu_item_restart};

    Menu menuRestart = Menu("Slot3", menu_restart_sure);


    std::vector<MenuItem> items;
    if (isFlycast2021())
    {
        items = {
            MenuItem("Resume", resume),
            MenuItem("Info", &menuInfo, fake),
            MenuItem("Settings", &menuSettings, fake),
            MenuItem("Credits", showCredit),
            MenuItem("Restart", &menuRestart, fake),
            MenuItem("Quit", &menuInfoQuit, fake),
        };
    }
    else
    {
        items = {
            MenuItem("Resume", resume),
            MenuItem("Info", &menuInfo, fake),
            MenuItem("Settings", &menuSettings, fake),
            MenuItem("Load/Save", &menuState, fake),
            MenuItem("Credits", showCredit),
            MenuItem("Restart", &menuRestart, fake),
            MenuItem("Quit", &menuInfoQuit, fake),
        };
    }

    Menu menu = Menu("Main Menu", items);

    menuManager.setCurrentMenu(&menu);
    // end menu
    auto frameDuration = duration_cast<nanoseconds>(seconds(1)) / max_fps;
    while (isRunning)
    {
        auto loopStart = high_resolution_clock::now();
        input_message = false;
        auto nextClock = std::chrono::high_resolution_clock::now();
        // double deltaTime = (nextClock - prevClock).count() / 1e9;
        bool realPause = pause_requested && input_pause_requested;
        bool showInfo = pause_requested && input_info_requested;
        if (input_info_requested)
        {
            // must poll to unpause
            totalFrames = 0; // reset total frames otherwise in next loop FPS are not accurate anymore
            core_input_poll();
            core_video_refresh(nullptr, 0, 0, 0);
            // std::this_thread::sleep_for(std::chrono::nanoseconds((int64_t)(10 * 1e6)));
            continue;
        }
        else if (realPause)
        {
            totalFrames = 0; // reset total frames otherwise in next loop FPS are not accurate anymore
            core_input_poll();
        }
        else
        {
            // in some cores (pcsx-rearmed for example) is not enough to put in pause immediatly when info request is done
            // we need to redraw the screen at least one time
            if (showInfo)
            {
                redrawInfo = false;
            }
            else
            {
                redrawInfo = true;
            }
            g_retro.retro_run();
        }

        // make sure each frame takes *at least* 1/60th of a second
        // auto frameClock = std::chrono::high_resolution_clock::now();
        // double deltaTime = (nextClock - prevClock).count() / 1e9;
        // printf("frame time: %.2lf ms\n", deltaTime * 1e3);
        // double sleepSecs = 1.0 / max_fps - deltaTime;

        // gettimeofday(&startTime, NULL);
        if (input_exit_requested)
        {
            isRunning = false;
        }
        else if (input_reset_requested)
        {
            input_reset_requested = false;
            g_retro.retro_reset();
        }

        auto loopEnd = high_resolution_clock::now();
        auto loopDuration = duration_cast<nanoseconds>(loopEnd - loopStart);

        // Calculate the remaining time to meet the frame duration
        auto sleepTime = frameDuration - loopDuration;

        // If the remaining time is positive, sleep to ensure fixed frame rate
        if ((runLoopAtDeclaredfps && sleepTime > nanoseconds::zero()) && !input_ffwd_requested)
        {
            std::this_thread::sleep_for(sleepTime * 0.99);
        }

        /*if ((runLoopAtDeclaredfps && sleepSecs > 0) && !input_ffwd_requested)
        {
            printf("WAITING...");
            std::this_thread::sleep_for(std::chrono::nanoseconds((int64_t)(sleepSecs * 1e9)));
        }*/
        prevClock = nextClock;
        totClock = std::chrono::high_resolution_clock::now();

        totalFrames++;
        elapsed += (totClock - nextClock).count() / 1e9;
        newFps = (int)(totalFrames / elapsed);
        retrorunLoopSkip = newFps;
        if (!startCalAvgFps)
        {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();

            if (elapsed_time >= 7) // we wait 7 seconds before starting to count the Average FPS, we want to be sure everything is up and running well
            {
                startCalAvgFps = true;
            }
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
            newFps = (int)(totalFrames / elapsed);
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
                fps = newFps; // > max_fps ? max_fps : newFps;

            if (opt_show_fps && elapsed >= 1.0)
            {
                logger.log(Logger::DEB, "FPS: %f", fps);
            }
            totalFrames = 0;
            elapsed = 0;
        }
    }
    logger.log(Logger::DEB, "Exiting from render loop...");
    logger.log(Logger::INF, "Saving sram into file:%", sramPath);
    SaveSram(sramPath);
    free(sramPath);
    sleep(2); // wait a little bit
    if (auto_save)
    {
        logger.log(Logger::INF, "Saving sav into file:%s", savePath);
        SaveState(savePath);
        free(savePath);
        sleep(1); // wait a little bit
    }
    // free(saveName);
    logger.log(Logger::INF, "Unloading core and deinit audio and video...");
    video_deinit();
    audio_deinit();
    // atexit(unload);

    pthread_t threadId;
    pthread_create(&threadId, NULL, &unload, NULL);

    sleep(1); // wait a little bit
    if (exitFlag == 0)
    { // if everything is ok we join the thread otherwise we exit without joining
        pthread_join(threadId, NULL);
    }
    else
    { // if it hangs we kill the thread and we exit
        pthread_kill(threadId, SIGUSR1);
        pthread_join(threadId, NULL);
        logger.log(Logger::INF, "Force exiting retrorun.");
        throw std::runtime_error("Force exiting retrorun.\n");
    }

    return 0;
}
