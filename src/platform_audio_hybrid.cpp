#include "audio_backend.h"

#include "globals.h"
#include "logger.h"

#include <algorithm>
#include <cctype>
#include <string>

struct rr_audio {
    const rr_audio_backend_api* api;
    void* native;
};

namespace {

std::string requested_backend = "auto";

std::string normalized(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

const rr_audio_backend_api* default_backend() {
#ifdef RR_AUDIO_BACKEND_GO2
    return rr_audio_backend_go2();
#elif defined(RR_AUDIO_BACKEND_SDL2)
    return rr_audio_backend_sdl2();
#else
    return nullptr;
#endif
}

const rr_audio_backend_api* resolve_backend(const std::string& requested) {
    const std::string value = normalized(requested);
    if (value.empty() || value == "auto" || value == "native")
        return default_backend();
#ifdef RR_AUDIO_BACKEND_GO2
    if (value == "go2" || value == "openal")
        return rr_audio_backend_go2();
#endif
#ifdef RR_AUDIO_BACKEND_SDL2
    if (value == "sdl" || value == "sdl2")
        return rr_audio_backend_sdl2();
#endif
    return nullptr;
}

} // namespace

bool rr_audio_backend_select(const char* backend) {
    const std::string value = backend ? backend : "auto";
    if (!resolve_backend(value))
        return false;
    requested_backend = normalized(value);
    return true;
}

const char* rr_audio_backend_name() {
    const rr_audio_backend_api* api = resolve_backend(requested_backend);
    return api ? api->name : "unavailable";
}

rr_audio_t* rr_audio_create(int frequency) {
    const rr_audio_backend_api* api = resolve_backend(requested_backend);
    if (!api) {
        logger.log(Logger::ERR, "No usable audio backend is available.");
        return nullptr;
    }
    void* native = api->create(frequency);
    if (!native)
        return nullptr;
    rr_audio_t* audio = new rr_audio_t{api, native};
    logger.log(Logger::INF, "Hybrid audio backend active: %s", api->name);
    return audio;
}

void rr_audio_destroy(rr_audio_t* audio) {
    if (!audio)
        return;
    audio->api->destroy(audio->native);
    delete audio;
}
bool rr_audio_submit(rr_audio_t* audio, const short* data, int frames) {
    return audio && audio->api->submit(audio->native, data, frames);
}
void rr_audio_release_thread(rr_audio_t* audio) {
    if (audio)
        audio->api->release_thread(audio->native);
}
bool rr_audio_valid(rr_audio_t* audio) {
    return audio && audio->api->valid(audio->native);
}
void rr_audio_flush(rr_audio_t* audio) {
    if (audio)
        audio->api->flush(audio->native);
}
void rr_audio_pause(rr_audio_t* audio, bool paused) {
    if (audio)
        audio->api->pause(audio->native, paused);
}
void rr_audio_cancel(rr_audio_t* audio) {
    if (audio)
        audio->api->cancel(audio->native);
}
void rr_audio_diagnostics_get(rr_audio_t* audio,
                              rr_audio_diagnostics_t* diagnostics) {
    if (!diagnostics)
        return;
    *diagnostics = {};
    if (audio)
        audio->api->diagnostics_get(audio->native, diagnostics);
}
uint32_t rr_audio_volume_get(rr_audio_t* audio, const char* control) {
    return audio ? audio->api->volume_get(audio->native, control) : 0;
}
void rr_audio_volume_set(rr_audio_t* audio, uint32_t value,
                         const char* control) {
    if (audio)
        audio->api->volume_set(audio->native, value, control);
}
