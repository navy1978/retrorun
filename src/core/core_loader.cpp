/*
retrorun - libretro frontend for Anbernic Devices
Copyright (C) 2020  OtherCrashOverride
Copyright (C) 2021-present  navy1978
*/

#include "core_loader.h"
#include "globals.h"
#include "config.h"
#include "keyboard.h"
#include "disk_control.h"
#include "achievements.h"
#include "video.h"
#include "audio.h"
#include "input.h"
#include "rumble.h"
#include "platform.h"

#include <dlfcn.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <exception>
#include <stdexcept>
#include <atomic>
#include <map>
#include <mutex>

#ifdef RR_PLATFORM_SDL
#include <SDL.h>
#endif

#define MAX_COUNTERS 16

// Defined in main.cpp
extern const char *opt_savedir;
extern const char *opt_systemdir;

#define RETRO_DEVICE_ATARI_JOYSTICK RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
#define RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER 56

// --- Extern variable definitions ---

RetroCore g_retro;
retro_hw_context_reset_t retro_context_reset;
retro_hw_context_reset_t retro_context_destroy;

// --- Internal state ---

static struct retro_perf_counter *perf_counters[MAX_COUNTERS];
static int perf_counter_count = 0;

typedef std::map<std::string, std::string> varmap_t;
static varmap_t variables;
static std::mutex pending_av_mutex;
static struct retro_system_av_info pending_av_info = {};
static bool pending_av_info_valid = false;

static bool registerCoreOptions(
    const struct retro_core_option_definition *options)
{
    if (!options)
        return false;

    for (std::size_t index = 0; options[index].key != nullptr; ++index)
    {
        const retro_core_option_definition &option = options[index];
        const char *defaultValue = option.default_value;
        if (!defaultValue && option.values[0].value)
            defaultValue = option.values[0].value;
        if (!defaultValue)
        {
            logger.log(Logger::WARN,
                       "Core option '%s' has no valid default value; ignored.",
                       option.key);
            continue;
        }
        variables[option.key] = defaultValue;
        logger.log(Logger::DEB, "OPTION: key=%s, value=%s",
                   option.key, defaultValue);
    }
    return true;
}

// --- Perf counters ---

retro_time_t cpu_features_get_time_usec(void)
{
    struct timespec tv;
    if (clock_gettime(CLOCK_MONOTONIC, &tv) < 0)
        return 0;
    return tv.tv_sec * INT64_C(1000000) + (tv.tv_nsec / 1000);
}

uint64_t cpu_features_get(void)
{
    uint64_t cpu = 0;

#if defined(__x86_64__) || defined(__i386__)
    uint32_t eax, ebx, ecx, edx;
    __asm__("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1));

    if (edx & (1 << 25)) cpu |= RETRO_SIMD_SSE;
    if (edx & (1 << 26)) cpu |= RETRO_SIMD_SSE2;
    if (ecx & (1 << 0))  cpu |= RETRO_SIMD_SSE3;
    if (ecx & (1 << 9))  cpu |= RETRO_SIMD_SSSE3;
    if (ecx & (1 << 19)) cpu |= RETRO_SIMD_SSE4;
    if (ecx & (1 << 28)) cpu |= RETRO_SIMD_AVX;

    __asm__("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(7), "c"(0));

    if (ebx & (1 << 5)) cpu |= RETRO_SIMD_AVX2;
#endif

#if defined(__ARM_NEON__) || defined(__aarch64__)
    cpu |= RETRO_SIMD_NEON;
#endif

    return cpu;
}

retro_perf_tick_t cpu_features_get_perf_counter(void)
{
    struct timespec tv;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &tv) == 0)
        return (retro_perf_tick_t)tv.tv_sec * 1000000000 + tv.tv_nsec;
    else if (clock_gettime(CLOCK_MONOTONIC, &tv) == 0)
        return (retro_perf_tick_t)tv.tv_sec * 1000000000 + tv.tv_nsec;
    return 0;
}

void runloop_performance_counter_register(struct retro_perf_counter *perf)
{
    if (perf->registered || perf_counter_count >= MAX_COUNTERS)
        return;
    perf_counters[perf_counter_count++] = perf;
    perf->registered = true;
}

void core_performance_counter_start(struct retro_perf_counter *perf)
{
    perf->call_cnt++;
    perf->start = cpu_features_get_perf_counter();
}

void core_performance_counter_stop(struct retro_perf_counter *perf)
{
    perf->total += cpu_features_get_perf_counter() - perf->start;
}

void runloop_perf_log(void)
{
    logger.log(Logger::DEB, "[PERF]: Performance counters:\n");
    for (int i = 0; i < perf_counter_count; i++)
    {
        struct retro_perf_counter *perf = perf_counters[i];
        logger.log(Logger::DEB, "%s: calls: %llu, total: %lld ns\n",
               perf->ident, (unsigned long long)perf->call_cnt, (long long)perf->total);
    }
}

// --- Helpers ---

#define load_sym(V, S)                                                         \
    do                                                                         \
    {                                                                          \
        if (!((*(void **)&V) = dlsym(g_retro.handle, #S)))                     \
        {                                                                      \
            logger.log(Logger::ERR,"[noarch] Failed to load symbol '" #S "'': %s", dlerror()); \
            exit(1);                                                           \
        }                                                                      \
    } while (0)

#define load_retro_sym(S) load_sym(g_retro.S, S)

#ifdef RR_PLATFORM_GO2
static retro_proc_address_t get_proc_address(const char *sym)
{
    retro_proc_address_t result = reinterpret_cast<retro_proc_address_t>(rr_context_get_proc_address(sym));
    logger.log(Logger::DEB, "get_proc_address: sym='%s', result=%p", sym, (void *)result);
    return result;
}
#endif

static void log_unhandled_environment_once(unsigned cmd)
{
    static std::atomic<uint64_t> logged_commands[16] = {};
    constexpr unsigned qualifier_mask =
        RETRO_ENVIRONMENT_EXPERIMENTAL | RETRO_ENVIRONMENT_PRIVATE;
    const unsigned command = cmd & ~qualifier_mask;
    const unsigned qualifier = (cmd & qualifier_mask) >> 16;
    const unsigned bit_index = qualifier * 256u + command;

    if (command >= 256u) {
        logger.log(Logger::DEB, "Unhandled env #%u", cmd);
        return;
    }

    const uint64_t mask = uint64_t{1} << (bit_index % 64u);
    const uint64_t previous = logged_commands[bit_index / 64u].fetch_or(
        mask, std::memory_order_relaxed);
    if ((previous & mask) == 0)
        logger.log(Logger::DEB, "Unhandled env #%u (further occurrences suppressed)", cmd);
}

// --- Environment callback ---

bool core_environment(unsigned cmd, void *data)
{
    bool *bval;
    switch (cmd)
    {
    case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
    {
        bval = (bool *)data;
        *bval = input_ffwd_requested;
        static bool first = true;
        static bool previous = false;
        if (first || previous != *bval) {
            logger.log(Logger::DEB, "Libretro GET_FASTFORWARDING -> %s",
                       *bval ? "true" : "false");
            first = false;
            previous = *bval;
        }
        return true;
    }

    case RETRO_ENVIRONMENT_SET_FASTFORWARDING_OVERRIDE:
        fastForwardSetOverride(
            static_cast<const retro_fastforwarding_override *>(data));
        return data != nullptr;

    case RETRO_ENVIRONMENT_GET_THROTTLE_STATE:
    {
        auto *state = static_cast<retro_throttle_state *>(data);
        if (!state)
            return false;
        if (pause_requested) {
            state->mode = RETRO_THROTTLE_FRAME_STEPPING;
            state->rate = 0.0f;
        } else if (input_ffwd_requested) {
            state->mode = RETRO_THROTTLE_FAST_FORWARD;
            const float ratio = fastForwardRatio();
            state->rate = ratio >= 1.0f ? originalFps * ratio : 0.0f;
        } else {
            state->mode = RETRO_THROTTLE_NONE;
            state->rate = originalFps;
        }
        return true;
    }

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
            logger.log(Logger::DEB, "RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: RR_PIXEL_FORMAT_RGBA5551");
            color_format = RR_PIXEL_FORMAT_RGBA5551;
            break;
        case RETRO_PIXEL_FORMAT_RGB565:
            logger.log(Logger::DEB, "RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: RR_PIXEL_FORMAT_RGB565");
            color_format = RR_PIXEL_FORMAT_RGB565;
            break;
        case RETRO_PIXEL_FORMAT_XRGB8888:
            logger.log(Logger::DEB, "RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: RR_PIXEL_FORMAT_XRGB8888");
            color_format = RR_PIXEL_FORMAT_XRGB8888;
            break;
        default:
            logger.log(Logger::DEB, "RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: FORMAT UNKNOWN");
            return false;
        }
        return true;
    }

    case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
    {
        logger.log(Logger::DEB, "Core supports Rumble.\n");
        struct retro_rumble_interface *iface = (struct retro_rumble_interface *)data;
        logger.log(Logger::DEB, "[Environ]: GET_RUMBLE_INTERFACE.\n");
        iface->set_rumble_state = retrorun_input_set_rumble;
        break;
    }

    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *(const char **)data = opt_systemdir;
        return true;

    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *(const char **)data = opt_savedir;
        return true;

    case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER:
    {
#ifdef RR_PLATFORM_SDL
        unsigned int *preferred = static_cast<unsigned int *>(data);
        if (sdlVideoRenderer == SDLVideoRenderer::Software) {
            *preferred = RETRO_HW_CONTEXT_NONE;
            logger.log(Logger::DEB, "SDL renderer preference: software");
        } else if (sdlVideoRenderer == SDLVideoRenderer::Vulkan) {
            *preferred = RETRO_HW_CONTEXT_VULKAN;
            logger.log(Logger::WARN, "SDL renderer preference: Vulkan (not available in this build)");
        } else {
#ifdef RR_SDL_GLES
            *preferred = RETRO_HW_CONTEXT_OPENGLES3;
            logger.log(Logger::DEB, "SDL renderer preference: OpenGL ES 3");
#else
            *preferred = RETRO_HW_CONTEXT_OPENGL_CORE;
            logger.log(Logger::DEB, "SDL renderer preference: OpenGL Core");
#endif
        }
        return true;
#else
        logger.log(Logger::DEB, "RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: preferred OPENGLES3");
        unsigned int *preferred = (unsigned int *)data;
        *preferred = RETRO_HW_CONTEXT_OPENGLES3;
        return true;
#endif
    }

    case RETRO_ENVIRONMENT_SET_HW_RENDER:
    {
#ifdef RR_PLATFORM_SDL
        retro_hw_render_callback *hw = static_cast<retro_hw_render_callback *>(data);
        if (sdlVideoRenderer == SDLVideoRenderer::Software) {
            logger.log(Logger::WARN, "SDL software renderer selected; rejecting hardware context type %d", hw->context_type);
            return false;
        }
        if (hw->context_type == RETRO_HW_CONTEXT_VULKAN ||
            sdlVideoRenderer == SDLVideoRenderer::Vulkan) {
            logger.log(Logger::ERR, "Vulkan requires a libretro Vulkan interface and MoltenVK; unavailable in this build");
            return false;
        }
#ifdef RR_SDL_GLES
        if (hw->context_type != RETRO_HW_CONTEXT_OPENGLES_VERSION &&
            hw->context_type != RETRO_HW_CONTEXT_OPENGLES3 &&
            hw->context_type != RETRO_HW_CONTEXT_OPENGLES2) {
            logger.log(Logger::WARN, "SDL OpenGL ES backend cannot provide requested context type %d", hw->context_type);
            return false;
        }
#else
        if (hw->context_type != RETRO_HW_CONTEXT_OPENGL &&
            hw->context_type != RETRO_HW_CONTEXT_OPENGL_CORE) {
            logger.log(Logger::WARN, "SDL OpenGL backend cannot provide requested context type %d", hw->context_type);
            return false;
        }
#endif
        isOpenGL = true;
        GLContextMajor = hw->version_major;
        GLContextMinor = hw->version_minor;
        retro_context_reset = hw->context_reset;
        retro_context_destroy = hw->context_destroy;
        hw->get_current_framebuffer = core_video_get_current_framebuffer;
        hw->get_proc_address = (retro_hw_get_proc_address_t)rr_context_get_proc_address;
        logger.log(Logger::DEB, "SDL HW render accepted: type=%d, version=%d.%d",
                   hw->context_type, GLContextMajor, GLContextMinor);
        return true;
#else
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
        retro_context_destroy = hw->context_destroy;

        hw->get_current_framebuffer = core_video_get_current_framebuffer;
        hw->get_proc_address = (retro_hw_get_proc_address_t)get_proc_address;
        logger.log(Logger::DEB, "HWRENDER: context_type=%d, major=%d, minor=%d\n",
                   hw->context_type, GLContextMajor, GLContextMinor);
        return true;
#endif
    }

    case RETRO_ENVIRONMENT_SET_VARIABLES:
    {
        retro_variable *var = (retro_variable *)data;
        if (!var)
            return false;
        while (var->key != NULL)
        {
            std::string key = var->key;
            if (!var->value)
            {
                logger.log(Logger::WARN,
                           "Legacy core option '%s' has no value definition; ignored.",
                           var->key);
                ++var;
                continue;
            }
            const char *separator = strchr(var->value, ';');
            if (!separator)
            {
                logger.log(Logger::WARN,
                           "Legacy core option '%s' is malformed; ignored.",
                           var->key);
                ++var;
                continue;
            }
            const char *start = separator + 1;
            while (*start == ' ' || *start == '\t')
                ++start;

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

    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
    {
        if (!data)
            return false;
        {
            std::lock_guard<std::mutex> lock(pending_av_mutex);
            pending_av_info = *static_cast<retro_system_av_info*>(data);
            pending_av_info_valid = true;
        }
        logger.log(Logger::DEB,
                   "RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO staged: %ux%u max=%ux%u fps=%.6f sample_rate=%.3f",
                   pending_av_info.geometry.base_width,
                   pending_av_info.geometry.base_height,
                   pending_av_info.geometry.max_width,
                   pending_av_info.geometry.max_height,
                   pending_av_info.timing.fps,
                   pending_av_info.timing.sample_rate);
        return true;
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
        if (!data) return false;
        *static_cast<unsigned*>(data) = 1;
        return true;
    }

    case RETRO_ENVIRONMENT_GET_PERF_INTERFACE:
    {
        struct retro_perf_callback *cb = (struct retro_perf_callback *)data;
        logger.log(Logger::DEB, "[Environ]: GET_PERF_INTERFACE.");
        cb->get_time_usec    = cpu_features_get_time_usec;
        cb->get_cpu_features = cpu_features_get;
        cb->get_perf_counter = cpu_features_get_perf_counter;
        cb->perf_register    = runloop_performance_counter_register;
        cb->perf_start       = core_performance_counter_start;
        cb->perf_stop        = core_performance_counter_stop;
        cb->perf_log         = runloop_perf_log;
        return true;
    }

    case RETRO_ENVIRONMENT_GET_LANGUAGE:
    {
        if (!data)
            return false;
        *static_cast<unsigned *>(data) = RETRO_LANGUAGE_ENGLISH;
        return true;
    }

    case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
    {
        const struct retro_keyboard_callback *cb =
            static_cast<const struct retro_keyboard_callback *>(data);
        if (cb && cb->callback)
        {
            rr_keyboard_set_callback(cb->callback);
            logger.log(Logger::DEB, "RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK: registered");
        }
        return true;
    }

    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
    {
        achievements_set_memory_map(static_cast<const retro_memory_map*>(data));
        return true;
    }

    case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
    {
        rr_disk_control_set(static_cast<const retro_disk_control_callback*>(data));
        logger.log(Logger::DEB, "RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE: registered");
        return true;
    }

    case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE:
    {
        rr_disk_control_set_ext(static_cast<const retro_disk_control_ext_callback*>(data));
        logger.log(Logger::DEB, "RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE: registered");
        return true;
    }

    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    {
        const struct retro_controller_info *arg = (retro_controller_info *)data;
        logger.log(Logger::DEB, "Controllers Available:");
        for (unsigned x = 0; x < arg->num_types; x++)
        {
            const struct retro_controller_description *type = &arg->types[x];
            logger.log(Logger::DEB, " -\t%s: %u", type->desc, type->id);
            controllerMap[type->id] = type->desc;
        }
        return true;
    }

    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
    {
        const retro_core_options_intl *options =
            static_cast<const retro_core_options_intl *>(data);
        // RetroRun currently exposes an English UI. Localised descriptions are
        // not displayed, but registering the US definitions is essential:
        // cores use the resulting defaults through GET_VARIABLE.
        return options && registerCoreOptions(options->us);
    }

    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
    {
        logger.log(Logger::DEB, "RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY not implemented");
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
        bool found = false;

        std::map<std::string, std::string>::iterator it = conf_map.find(var->key);
        if (it != conf_map.end())
        {
            logger.log(Logger::DEB, "key found: %s  value: %s", it->first.c_str(), it->second.c_str());

            if (it->first.compare("flycast_internal_resolution") == 0 || it->first.compare("flycast2021_internal_resolution") == 0 || it->first.compare("parallel-n64-screensize") == 0)
            {
                if (it->second.compare("320x240") == 0)
                    resolution = R_320_240;
                else if (it->second.compare("640x480") == 0)
                    resolution = R_640_480;
            }

            var->value = it->second.c_str();
            found = true;
            return true;
        }
        if (!found)
        {
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

    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
    {
        unsigned int *options_version = (unsigned int *)data;
        *options_version = 1;
        return true;
    }

    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
    {
        return registerCoreOptions(
            static_cast<const retro_core_option_definition *>(data));
    }

    default:
        log_unhandled_environment_once(cmd);
        return false;
    }

    return true;
}

// --- Core load ---

void core_load(const char *sofile)
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

    g_retro.callbacks_enabled = true;
    g_retro.retro_init();
    g_retro.initialized = true;
    logger.log(Logger::DEB, "Core loaded.");

    struct retro_system_info system = {0, 0, 0, false, false};
    g_retro.retro_get_system_info(&system);

    logger.log(Logger::DEB, "Core Info: library_name='%s'", system.library_name);
    logger.log(Logger::DEB, "Core Info: library_version='%s'", system.library_version);
    logger.log(Logger::DEB, "Core Info: can extract zip files='%s'", system.block_extract ? "true" : "false");

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

// --- Core load game ---

void core_load_game(const char *filename)
{
    struct retro_system_timing timing = {60.0f, 10000.0f};
    struct retro_game_geometry geom = {100, 100, 100, 100, 1.0f};
    struct retro_system_av_info av = {geom, timing};
    struct retro_system_info system = {0, 0, 0, false, false};
    struct retro_game_info info = {filename, 0, 0, NULL};

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
        fclose(file);
        free(const_cast<void *>(info.data));
        logger.log(Logger::ERR, "The core failed to load the content.");
        exit(1);
    }

    g_retro.game_loaded = true;
    fclose(file);
    free(const_cast<void *>(info.data));

    g_retro.retro_get_system_av_info(&av);
    core_clear_pending_av_info();
    video_configure(&av.geometry);
    audio_init(av.timing.sample_rate, av.timing.fps);
    return;

libc_error:
    if (file)
        fclose(file);
    free(const_cast<void *>(info.data));
    logger.log(Logger::ERR, "Failed to load content '%s'", filename);
    exit(1);
}

// --- Core unload ---

void *core_unload(void *)
{
    core_deinit();
    core_close();
    return nullptr;
}

void core_deinit()
{
    core_disable_callbacks();
    rr_keyboard_clear_callback();
    rr_disk_control_clear();
    core_unload_game();
    if (g_retro.initialized)
    {
        logger.log(Logger::DEB, "Core lifecycle: calling retro_deinit");
        g_retro.retro_deinit();
        g_retro.initialized = false;
        logger.log(Logger::DEB, "Core lifecycle: retro_deinit completed");
    }
}

void core_unload_game()
{
    if (g_retro.game_loaded && g_retro.retro_unload_game)
    {
        // The legacy Flycast 2021 core shipped by ArkOS/dArkOS crashes inside
        // retro_unload_game() on both GO2 and SDL2.  The historical GO2 path
        // intentionally went straight to retro_deinit(), which releases the
        // same content-owned state without taking the faulty entry point.
        // Keep the normal exactly-once lifecycle for every other core.
        if (isFlycast2021())
        {
            logger.log(Logger::WARN,
                       "Legacy Flycast shutdown quirk: skipping crashing retro_unload_game; retro_deinit will release content state");
            g_retro.game_loaded = false;
            return;
        }
        logger.log(Logger::DEB, "Core lifecycle: calling retro_unload_game");
        g_retro.retro_unload_game();
        g_retro.game_loaded = false;
        logger.log(Logger::DEB, "Core lifecycle: retro_unload_game completed");
    }
}

void core_close()
{
    if (g_retro.handle)
    {
        dlclose(g_retro.handle);
        g_retro.handle = nullptr;
    }
    retro_context_reset = nullptr;
    retro_context_destroy = nullptr;
    core_clear_pending_av_info();
}

void core_disable_callbacks()
{
    g_retro.callbacks_enabled = false;
}

bool core_callbacks_enabled()
{
    return g_retro.callbacks_enabled;
}

bool core_take_pending_av_info(struct retro_system_av_info* info)
{
    if (!info)
        return false;
    std::lock_guard<std::mutex> lock(pending_av_mutex);
    if (!pending_av_info_valid)
        return false;
    *info = pending_av_info;
    pending_av_info_valid = false;
    return true;
}

void core_clear_pending_av_info()
{
    std::lock_guard<std::mutex> lock(pending_av_mutex);
    pending_av_info_valid = false;
    pending_av_info = {};
}

bool core_reset_synchronized()
{
    if (!g_retro.initialized || !g_retro.retro_reset)
        return false;
    audio_flush();
    video_synchronize();
    g_retro.retro_reset();
    return true;
}
