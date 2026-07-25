#pragma once

#include "platform.h"

// Internal audio backend contract. The public frontend continues to use the
// rr_audio_* API from platform.h; hybrid builds select one of these providers
// once, before the core initializes audio.
struct rr_audio_backend_api {
    const char* name;
    void* (*create)(int frequency);
    void (*destroy)(void* audio);
    bool (*submit)(void* audio, const short* data, int frames);
    void (*release_thread)(void* audio);
    bool (*valid)(void* audio);
    void (*flush)(void* audio);
    void (*pause)(void* audio, bool paused);
    void (*cancel)(void* audio);
    void (*diagnostics_get)(void* audio, rr_audio_diagnostics_t* diagnostics);
    uint32_t (*volume_get)(void* audio, const char* control);
    void (*volume_set)(void* audio, uint32_t value, const char* control);
};

#ifdef RR_AUDIO_BACKEND_GO2
const rr_audio_backend_api* rr_audio_backend_go2();
#endif

#ifdef RR_AUDIO_BACKEND_SDL2
const rr_audio_backend_api* rr_audio_backend_sdl2();
#endif
