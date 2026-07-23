#pragma once

/*
retrorun - libretro frontend for Anbernic Devices
Copyright (C) 2020  OtherCrashOverride
Copyright (C) 2021-present  navy1978
*/

#include "libretro.h"
#include <cstddef>

// The g_retro struct holds all dlsym'd libretro entry points
struct RetroCore {
    void *handle;
    bool initialized;
    bool game_loaded;
    bool callbacks_enabled;

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
    bool (*retro_load_game)(const struct retro_game_info *game);
    void (*retro_unload_game)(void);
    void *(*retro_get_memory_data)(unsigned id);
    size_t (*retro_get_memory_size)(unsigned id);
};

extern RetroCore g_retro;

// Perf counter interface
retro_time_t cpu_features_get_time_usec(void);
uint64_t cpu_features_get(void);
retro_perf_tick_t cpu_features_get_perf_counter(void);
void runloop_performance_counter_register(struct retro_perf_counter *perf);
void core_performance_counter_start(struct retro_perf_counter *perf);
void core_performance_counter_stop(struct retro_perf_counter *perf);
void runloop_perf_log(void);

// Core lifecycle
void core_load(const char *sofile);
void core_load_game(const char *filename);
void *core_unload(void *);
void core_deinit();
void core_unload_game();
void core_close();
void core_disable_callbacks();
bool core_callbacks_enabled();
bool core_take_pending_av_info(struct retro_system_av_info* info);
void core_clear_pending_av_info();
bool core_reset_synchronized();

// The libretro environment callback
bool core_environment(unsigned cmd, void *data);

// HW render callbacks (set by core_environment)
extern retro_hw_context_reset_t retro_context_reset;
extern retro_hw_context_reset_t retro_context_destroy;
