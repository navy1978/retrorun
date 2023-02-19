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

#include <unistd.h>

//#include <go2/queue.h>

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
/* unsigned * --
 *
 * Allows an implementation to ask frontend preferred hardware
 * context to use. Core should use this information to deal
 * with what specific context to request with SET_HW_RENDER.
 *
 * 'data' points to an unsigned variable
 */

// extern go2_battery_state_t batteryState;

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
const char *ws = " \t\n\r\f\v";

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
            std::cout << "-RR- Error reading configuration file, key: " << key << "\n";
        }
    }
    std::cout << "-RR- --- Configuration loaded! ---\n"
              << std::endl;
    // std::cout << "After init ====>mymap.size() is " << conf_map.size() << '\n';
    file_in.close();
}

static void core_log(enum retro_log_level level, const char *fmt, ...)
{
    char buffer[4096] = {
        0};

    static const char *levelstr[] = {
        "dbg",
        "inf",
        "wrn",
        "err"};

    va_list va;

    va_start(va, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, va);
    va_end(va);

    if (level == 0)
        return;

    fprintf(stdout, "<-- %s --> [%s] %s ", coreName.c_str(), levelstr[level], buffer);
    fflush(stdout);

#if 0
	if (level == RETRO_LOG_ERROR)
		exit(EXIT_FAILURE);
#endif
}

static __eglMustCastToProperFunctionPointerType get_proc_address(const char *sym)
{
    __eglMustCastToProperFunctionPointerType result = eglGetProcAddress(sym);
    // printf("get_proc_address: sym='%s', result=%p\n", sym, (void*)result);

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
        cb->log = core_log;
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
            printf("-RR- RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: DRM_FORMAT_RGBA5551\n");
            color_format = DRM_FORMAT_RGBA5551;
            break;

        case RETRO_PIXEL_FORMAT_RGB565:
            printf("-RR- RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: DRM_FORMAT_RGB565\n");
            color_format = DRM_FORMAT_RGB565;
            break;

        case RETRO_PIXEL_FORMAT_XRGB8888:
            printf("-RR- RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: DRM_FORMAT_XRGB8888\n");
            color_format = DRM_FORMAT_XRGB8888;
            break;

        default:
            printf("-RR- RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: FORMAT UNKNOWN\n");
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
        unsigned int *preferred = (unsigned int *)data;
        *preferred = RETRO_HW_CONTEXT_OPENGLES3;
        return true;
    }

    case RETRO_ENVIRONMENT_SET_HW_RENDER:
    {
        retro_hw_render_callback *hw = (retro_hw_render_callback *)data;

        printf("-RR- RETRO_ENVIRONMENT_SET_HW_RENDER: context_type=%d\n", hw->context_type);

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

        printf("-RR- HWRENDER: context_type=%d, major=%d, minor=%d\n",
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
            printf("-RR- -> SET_VAR: %s=%s\n", key.c_str(), value.c_str());
            ++var;
        }

        break;
    }


    case RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE:
    {
        float *var = (float *)data;
        *var = 60;
          printf("-RR- -> SETTING REFRESH RATE CALLED!\n");
        return true;

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
            printf("-RR- key found: %s  value: %s\n", it->first.c_str(), it->second.c_str());

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
                printf("-RR- ENV_VAR (default): %s=%s\n", var->key, var->value);

                return true;
            }
        }

        return false;
    }

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
        while (options[i].key != 0)
        {
            std::string key = options[i].key;
            std::string value = options[i].default_value;

            variables[key] = value;

            printf("-RR- OPTION: key=%s, value=%s\n", key.c_str(), value.c_str());
            ++i;
        }

        return true;
    }

    default:
        core_log(RETRO_LOG_DEBUG, "Unhandled env #%u", cmd);
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
        printf("-RR- Failed to load core: %s\n", dlerror());
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

    g_retro.retro_init();
    g_retro.initialized = true;

    printf("-RR- Core loaded\n");

    // we postpone this call later because some emulators dont like it (dosbox-core)
    //g_retro.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);

    struct retro_system_info system = {
        0, 0, 0, false, false};

    g_retro.retro_get_system_info(&system);

    printf("-RR- core_load: library_name='%s'\n", system.library_name);

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
    printf("Core:'%s'\n", system.library_name);

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
        printf("-RR- The core failed to load the content.\n");
        abort();
    }

    g_retro.retro_get_system_av_info(&av);

    video_configure(&av.geometry);
    audio_init(av.timing.sample_rate);

    return;

libc_error:
    printf("-RR- Failed to load content '%s'\n", filename);
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

    return isFlycast() ? RETRO_MEMORY_VIDEO_RAM : RETRO_MEMORY_SAVE_RAM;
}

static int LoadState(const char *saveName)
{
    FILE *file = fopen(saveName, "rb");
    if (!file)
    {
        printf("-RR- Error loading state: File '%s' not found!\n", saveName);
        return -1;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    if (size < 1)
    {
        printf("-RR- Error loading state, in file '%s': size is wrong!\n", saveName);
        return -1;
    }
    void *ptr = malloc(size);
    if (!ptr)
    {
        abort();
    }
    size_t count = fread(ptr, 1, size, file);

    if ((size_t)size != count)
    {
        printf("-RR- Error loading state, in file '%s': size mismatch!\n", saveName);
        free(ptr);
        abort();
    }
    fclose(file);
    g_retro.retro_unserialize(ptr, size);
    free(ptr);
    printf("-RR- File '%s': loaded correctly!\n", saveName);
    return 0;
}

static int LoadSram(const char *saveName)
{
    try
    {

        FILE *file = fopen(saveName, "rb");
        if (!file)
        {
            printf("-RR- Error loading sram: File '%s' not found!\n", saveName);
            return -1;
        }

        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        rewind(file);

        size_t sramSize = g_retro.retro_get_memory_size(getRetroMemory());
        if (size < 1)
        {
            printf("-RR- Error loading sram, memory size wrong!\n");
            return -1;
        }

        if (size != (long)sramSize)
        {
            printf("-RR- Error loading sram, in file '%s': size mismatch!\n", saveName);
            return -1;
        }
        void *ptr = g_retro.retro_get_memory_data(getRetroMemory());
        if (!ptr)
        {
            printf("-RR- Error loading sram, file '%s': contains wrong memory data!\n", saveName);
            abort();
        }
        size_t count = fread(ptr, 1, size, file);
        if ((size_t)size != count)
        {
            printf("-RR- Error loading sram, in file '%s': size mismatch!\n", saveName);
            abort();
        }
        fclose(file);
        printf("-RR- File '%s': loaded correctly!\n", saveName);
    }
    catch (const std::exception &e) // caught by reference to base
    {
        std::cout << " a standard exception was caught, with message '"
                  << e.what() << "'\n";
    }
    return 0;
}

static void SaveState(const char *saveName)
{
    size_t size = g_retro.retro_serialize_size();
    void *ptr = malloc(size);
    if (!ptr)
    {
        printf("-RR- Error saving state: ptr not valid!\n");
        abort();
    }
    g_retro.retro_serialize(ptr, size);
    FILE *file = fopen(saveName, "wb");
    if (!file)
    {
        printf("-RR- Error saving state: File '%s' cannot be opened!\n", saveName);
        free(ptr);
        abort();
    }
    size_t count = fwrite(ptr, 1, size, file);
    if (count != size)
    {
        printf("Error saving state: File '%s' count not valid!\n", saveName);
        free(ptr);
        abort();
    }
    fclose(file);
    free(ptr);
    printf("-RR- File '%s': saved correctly!\n", saveName);

    return;
}

static void SaveSram(const char *saveName)
{
    size_t size = g_retro.retro_get_memory_size(getRetroMemory());
    if (size < 1)
    {
        printf("-RR- nothing to save in srm file!, %zu\n", size);
        return;
    }
    void *ptr = g_retro.retro_get_memory_data(getRetroMemory());
    if (!ptr)
    {
        printf("-RR- Error saving sram: ptr not valid!\n");
        abort();
    }

    FILE *file = fopen(saveName, "wb");
    if (!file)
    {
        printf("-RR- Error saving sram: File '%s' cannot be opened!\n", saveName);
        abort();
    }

    size_t count = fwrite(ptr, 1, size, file);
    if (count != size)
    {
        printf("-RR- Error saving sram: File '%s' count not valid!\n", saveName);
        abort();
    }

    fclose(file);
    printf("-RR- saved!\n");
}

std::string getSystemFromRomPath(const char *fullpath)
{
    std::string arg_rom_string(fullpath);
    size_t slash = arg_rom_string.find_last_of("\\/");
    std::string dirPath = (slash != std::string::npos) ? arg_rom_string.substr(0, slash) : arg_rom_string;
    size_t slash2 = dirPath.find_last_of("\\/");
    std::string system = (slash2 != std::string::npos) ? dirPath.substr(slash2 + 1, dirPath.length()) : dirPath;
    printf("-RR- system='%s'\n", system.c_str());
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

void initConfig()
{
    std::ifstream infile(opt_setting_file);
    if (!infile.good())
    {
        printf("-RR- ERROR! Configuration file:'%s' doesn't exist default core settings will be used\n", opt_setting_file);
    }
    else
    {
        printf("-RR- Reading configuration file:'%s'\n", opt_setting_file);
        initMapConfig(opt_setting_file);
        try
        {
            const std::string &ssFolderValue = conf_map.at("retrorun_screenshot_folder");
            screenShotFolder = ssFolderValue;
            printf("-RR - Info - screenshot folder:%s\n",screenShotFolder.c_str());
        }
        catch (...)
        {
            printf("-RR- Warning: retrorun_screenshot_folder parameter not found in retrorun.cfg using default folder (/storage/roms/screenshots).\n");
            screenShotFolder = "/storage/roms/screenshots";
        }

        try
        {
            const std::string &ssFps_counter = conf_map.at("retrorun_fps_counter");
            input_fps_requested = ssFps_counter == "enabled" ? true : false;
            printf("-RR - Info - retrorun_fps_counter :%s\n",input_fps_requested ? "TRUE": "FALSE");
        }
        catch (...)
        {
            printf("-RR- Warning: retrorun_fps_counter parameter not found in retrorun.cfg using defaulf value (disabled).\n");
            input_fps_requested = false;
        }

        if (opt_aspect != 0.0f)
        {
            printf("Info: aspect_ratio forced from command line.\n");
        }
        else
        {
            try
            {
                const std::string &arValue = conf_map.at("retrorun_aspect_ratio");
                opt_aspect = getAspectRatio(arValue);
                printf("-RR - Info - retrorun_aspect_ratio :%f\n",opt_aspect);
            }
            catch (...)
            {
                printf("-RR- Warning: retrorun_aspect_ratio parameter not found in retrorun.cfg using default value (core provided).\n");
            }
        }
        try
        {
            const std::string &asValue = conf_map.at("retrorun_auto_save");
            auto_save = asValue == "true" ? true : false;
            printf("-RR - Info - Autosave: %s.\n", auto_save ? "true" : "false");
        }
        catch (...)
        {
            printf("-RR- Warning: retrorun_auto_save parameter not found in retrorun.cfg using default value (%s).\n", auto_save ? "true" : "false");
        }

        try
        {
            const std::string &lasValue = conf_map.at("retrorun_force_left_analog_stick");

            force_left_analog_stick = lasValue == "true" ? true : false;
            printf("-RR- Info - Force analog stick: %s.\n", force_left_analog_stick ? "true" : "false");
        }
        catch (...)
        {
            printf("-RR- Warning: retrorun_force_left_analog_stick parameter not found in retrorun.cfg using default value (%s).\n", force_left_analog_stick ? "true" : "false");
        }

        try
        {
            const std::string &tflValue = conf_map.at("retrorun_loop_60_fps");

            runLoopAt60fps = tflValue == "false" ? false : true;
            printf("-RR- Info - loop_60_fps: %s.\n", runLoopAt60fps ? "true" : "false");
        }
        catch (...)
        {
            printf("-RR- Warning: retrorun_loop_60_fps parameter not found in retrorun.cfg using default value (%s).\n", runLoopAt60fps ? "true" : "false");
        }


        try
        {
            const std::string &audioBufferValue = conf_map.at("retrorun_audio_buffer");
            if (!audioBufferValue.empty())
            {
                retrorun_audio_buffer = stoi(audioBufferValue);
                printf("-RR- Info - retrorun_audio_buffer: %d.\n", retrorun_audio_buffer);
            }
        }
        catch (...)
        {
            printf("-RR- Info: retrorun_audio_buffer parameter not found in retrorun.cfg using default value (-1).\n");
        }

         try
        {
            const std::string &mouseSpeedValue = conf_map.at("retrorun_mouse_speed_factor");
            if (!mouseSpeedValue.empty())
            {
                retrorun_mouse_speed_factor = stoi(mouseSpeedValue);
                printf("-RR- Info - retrorun_mouse_speed_factor: %d.\n", retrorun_mouse_speed_factor);
            }
        }
        catch (...)
        {
            printf("-RR- Info: retrorun_mouse_speed_factor parameter not found in retrorun.cfg using default value (5).\n");
        }

        processVideoInAnotherThread = isRG552() ? true : false;

        adaptiveFps = false;

        printf("-RR- Configuration initialized.\n");
    }

    infile.close();
}

int main(int argc, char *argv[])
{
    // printf("argc=%d, argv=%p\n", argc, argv);

    getDeviceName(); // we need this call here (otherwise it doesnt work because the methos is called only later , this need to be refactored)
    initConfig();

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
            printf("using '-n' as parameter, forces left analog stick to false!.\n");
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
            printf("Unknown option. '%s'\n", longopts[option_index].name);
            exit(EXIT_FAILURE);
        }
    }



    // gpio_joypad normally is false and should be set to true only for MP and 552
    // but the parameter sesetnd via command line wins so if that is true we leave it true
    if (!gpio_joypad){
        if (isRG351MP() || isRG552()){
            gpio_joypad= true;
        }
    }

    int remaining_args = argc - optind;
    int remaining_index = optind;
    printf("remaining_args=%d\n", remaining_args);

    if (remaining_args < 2)
    {
        printf("Usage: %s [-s savedir] [-d systemdir] [-a aspect] core rom\n\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // return 0;
    if (optind < argc)
    {
        printf("non-option ARGV-elements: ");
        while (optind < argc)
            printf("%s ", argv[optind++]);
        putchar('\n');
    }

    arg_core = argv[remaining_index++];
    arg_rom = argv[remaining_index++];

    // Overrides
    printf("Checking overrides.\n");

    input_gamepad_read();

    core_load(arg_core);
    // conf_map.clear();

    // on MP is reverted (4:3 -> 3:4)
    /*if (isSwanStation())
    {
        printf("-RR- isSwanStation.\n");
        opt_aspect = 0.75f;
    }*/

    core_load_game(arg_rom);
    // conf_map.clear();

    go2_input_state_t *gamepadState = input_gampad_current_get();
    if (go2_input_state_button_get(gamepadState, Go2InputButton_F1) == ButtonState_Pressed)
    {
        printf("-RR- Forcing restart due to button press (F1).\n");
        opt_restart = true;
    }

    // State
    char *sramPath = NULL;
    char *savePath = NULL;

    const char *fileName = FileNameFromPath(arg_rom);

    std::string fullNameString(fileName);
    // removing extension
    size_t lastindex = fullNameString.find_last_of(".");
    std::string rawname = fullNameString.substr(0, lastindex);
    romName = rawname;

    std::string system = getSystemFromRomPath(arg_rom);

    std::string srmAutoPath = "/storage/roms/<system>/<gameName>.srm";
    std::string tempSrmPath = replace(srmAutoPath, "<system>", system);
    std::string srmPathFinal = replace(tempSrmPath, "<gameName>", romName);

    std::string stateAutoPath = "/storage/roms/savestates/<system>/<gameName>.rrstate.auto";
    std::string tempStatePath = replace(stateAutoPath, "<system>", system);
    std::string statePathFinal = replace(tempStatePath, "<gameName>", romName);

    std::string sramFileName = rawname + ".srm";
    std::string savFileName = rawname + ".sav";

    // sramPath = PathCombine(opt_savedir, sramFileName.c_str());
    sramPath = (char *)malloc(srmPathFinal.length() + 1);
    sramPath = strcpy(sramPath, const_cast<char *>(srmPathFinal.c_str()));
    printf("-RR- sramPath='%s'\n", sramPath);
    savePath = (char *)malloc(statePathFinal.length() + 1);
    savePath = strcpy(savePath, const_cast<char *>(statePathFinal.c_str()));
    printf("-RR- savePath='%s'\n", savePath);

    if (opt_restart)
    {
        printf("-RR- Restarting.\n");
    }
    else
    {
        if (auto_save)
        {
            printf("-RR- Loading saved state - File '%s' \n", savePath);
            LoadState(savePath);
        }
    }
    printf("-RR- Loading sram - File '%s' \n", sramPath);
    LoadSram(sramPath);

    printf("-RR- Entering render loop.\n");

    // const char* batteryStateDesc[] = { "UNK", "DSC", "CHG", "FUL" };

    // struct timeval startTime;
    // struct timeval endTime;
    double elapsed = 0;
    int totalFrames = 0;
    bool isRunning = true;
    // sleep(1); // some cores (like yabasanshiro) from time to time hangs on retro_run otherwise

    struct retro_system_av_info info;
    g_retro.retro_get_system_av_info(&info);
    printf("-RR- System Info - aspect_ratio: %f\n", info.geometry.aspect_ratio);
    printf("-RR- System Info - base_width: %d\n", info.geometry.base_width);
    printf("-RR- System Info - base_height: %d\n", info.geometry.base_height);
    printf("-RR- System Info - max_width: %d\n", info.geometry.max_width);
    printf("-RR- System Info - max_height: %d\n", info.geometry.max_height);

    printf("-RR- System Info - fps: %f\n", info.timing.fps);
    printf("-RR- System Info - sample_rate: %f\n", info.timing.sample_rate);
    auto prevClock = std::chrono::high_resolution_clock::now();
    auto totClock = std::chrono::high_resolution_clock::now();
    double max_fps = info.timing.fps;
    double previous_fps = 0;
    originalFps = info.timing.fps;
    // adaptiveFps = isFlycast() ? true: false;
    bool redrawInfo = true;
    // we postpone this here because if we do it before some emulators dont like it (Dosbox core)
    g_retro.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
    while (isRunning)
    {
        auto nextClock = std::chrono::high_resolution_clock::now();
        // double deltaTime = (nextClock - prevClock).count() / 1e9;
        bool realPause =pause_requested && input_pause_requested;
        bool showInfo = pause_requested && input_info_requested;
        if (realPause || (showInfo && !redrawInfo))
        {
            // must poll to unpause
            totalFrames = 0; // reset total frames otherwise in next loop FPS are not accurate anymore
            core_input_poll();

        }
        else
        {
            // in some cores (pcsx-rearmed for example) is not enough to put in pause immediatly when info request is done
            // we need to redraw the screen at least one time
            if (showInfo){
               redrawInfo = false;
            }else {
               redrawInfo = true;
            }
            g_retro.retro_run();
        }

        // make sure each frame takes *at least* 1/60th of a second
        // auto frameClock = std::chrono::high_resolution_clock::now();
        double deltaTime = (nextClock - prevClock).count() / 1e9;
        // printf("frame time: %.2lf ms\n", deltaTime * 1e3);
        double sleepSecs = 1.0 / max_fps - deltaTime;

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

        if ((runLoopAt60fps && sleepSecs > 0) && !input_ffwd_requested)
        {
            //printf("-RR- waiting!\n");
            std::this_thread::sleep_for(std::chrono::nanoseconds((int64_t)(sleepSecs * 1e9)));
        }
        prevClock = nextClock;
        totClock = std::chrono::high_resolution_clock::now();
        
            totalFrames++;
            elapsed += (totClock - nextClock).count() / 1e9;
            newFps = (int)(totalFrames / elapsed);

            retrorunLoopCounter++;
            bool drawFps = false;
            if (retrorunLoopCounter == retrorunLoopSkip)
            {
                drawFps = true;
                newFps = (int)(totalFrames / elapsed);
                retrorunLoopCounter = 0;
            }
            if (adaptiveFps && !input_ffwd_requested)
            {

                if (previous_fps <= newFps)
                {
                    max_fps = newFps < info.timing.fps /2 ? (info.timing.fps/2) + 10 : info.timing.fps;
                    max_fps = newFps < info.timing.fps *2/3 ? (info.timing.fps *2/3) +5: info.timing.fps;
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
                fps = newFps > max_fps ? max_fps : newFps;

                if (opt_show_fps && elapsed >= 1.0)
                {
                    printf("-RR- FPS: %f\n", fps);
                }
                totalFrames = 0;
                elapsed = 0;
            }
        
    }

    printf("-RR- Exiting from render loop...\n");

    printf("-RR- Saving sram into file:%s\n", sramPath);
    SaveSram(sramPath);
    free(sramPath);
    sleep(2); // wait a little bit
    if (auto_save)
    {
        printf("-RR- Saving sav into file:%s\n", savePath);
        SaveState(savePath);
        free(savePath);
        sleep(1); // wait a little bit
    }
    // free(saveName);
    printf("-RR- Unloading core and deinit audio and video...\n");
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
        throw std::runtime_error("Force exiting retrorun.\n");
    }

    return 0;
}
