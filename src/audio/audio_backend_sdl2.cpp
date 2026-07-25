#include "audio_backend.h"

#include "globals.h"

#include <SDL.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {

struct sdl_audio {
    SDL_AudioDeviceID device = 0;
    std::atomic<int> volume{100};
    int frequency = 0;
    int period_frames = 0;
    int target_queue_ms = 0;
    int prefill_ms = 0;
    uint64_t prefill_frames = 0;
    int stretch_percent = 0;
    int stretch_low_ms = 0;
    uint64_t stretch_low_frames = 0;
    bool started = false;
    std::atomic<bool> cancelled{false};
    std::atomic<bool> paused{false};
    std::atomic<uint64_t> underruns{0};
    std::atomic<uint64_t> overruns{0};
    std::atomic<uint64_t> frames_dropped{0};
    std::atomic<uint64_t> max_queue_depth{0};
    std::atomic<uint64_t> min_queue_depth{std::numeric_limits<uint64_t>::max()};
    std::atomic<uint64_t> queue_depth_samples{0};
    std::atomic<uint64_t> queue_depth_total_frames{0};
    std::atomic<uint64_t> queue_empty_observations{0};
    std::atomic<uint64_t> queue_low_observations{0};
    std::atomic<uint64_t> adaptive_stretch_frames{0};
    bool submitted_once = false;
    std::vector<short> mix_buffer;
    std::vector<short> stretch_buffer;
    std::vector<short> silence_buffer;
};

int environment_milliseconds(const char* name, int fallback, int maximum) {
    const char* value = std::getenv(name);
    if (!value || !*value)
        return fallback;
    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    if (end == value || *end != '\0')
        return fallback;
    return static_cast<int>(std::clamp<long>(parsed, 0, maximum));
}

bool ensure_audio_subsystem() {
    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == SDL_INIT_AUDIO)
        return true;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0)
        return true;
    std::fprintf(stderr, "RetroRun SDL2 audio initialization failed: %s\n",
                 SDL_GetError());
    return false;
}

void* create(int frequency) {
    if (!ensure_audio_subsystem())
        return nullptr;

    SDL_AudioSpec wanted = {};
    wanted.freq = frequency;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = 2;
    wanted.samples = retrorun_audio_stable_buffer ? 2048 : 1024;
    SDL_AudioSpec obtained = {};

    sdl_audio* audio = new sdl_audio();
    audio->device = SDL_OpenAudioDevice(nullptr, 0, &wanted,
        retrorun_audio_stable_buffer ? &obtained : nullptr, 0);
    if (!audio->device) {
        std::fprintf(stderr, "RetroRun SDL2 audio open failed: %s\n", SDL_GetError());
        delete audio;
        return nullptr;
    }

    audio->frequency = retrorun_audio_stable_buffer ? obtained.freq : frequency;
    audio->period_frames = retrorun_audio_stable_buffer
        ? obtained.samples : wanted.samples;
    audio->target_queue_ms = environment_milliseconds(
        "RETRORUN_SDL_AUDIO_TARGET_MS",
        retrorun_audio_stable_buffer ? 140 : 80, 500);
    audio->prefill_ms = retrorun_audio_stable_buffer ? 0 :
        environment_milliseconds("RETRORUN_SDL_AUDIO_PREFILL_MS", 0, 200);
    audio->prefill_frames = static_cast<uint64_t>(audio->frequency) *
        static_cast<uint64_t>(audio->prefill_ms) / 1000;
    audio->stretch_percent = retrorun_audio_stable_buffer ? 0 :
        environment_milliseconds("RETRORUN_SDL_AUDIO_STRETCH_PERCENT",
                                 retrorun_sdl_audio_stretch_percent, 10);
    audio->stretch_low_ms = environment_milliseconds(
        "RETRORUN_SDL_AUDIO_STRETCH_LOW_MS",
        retrorun_sdl_audio_stretch_low_ms, 200);
    // Publish the resolved values so benchmark metadata records environment
    // overrides and stable-buffer suppression rather than only raw config.
    retrorun_sdl_audio_stretch_percent = audio->stretch_percent;
    retrorun_sdl_audio_stretch_low_ms = audio->stretch_low_ms;
    audio->stretch_low_frames = static_cast<uint64_t>(audio->frequency) *
        static_cast<uint64_t>(audio->stretch_low_ms) / 1000;
    audio->started = !retrorun_audio_stable_buffer && audio->prefill_frames == 0;
    audio->silence_buffer.assign(static_cast<size_t>(audio->period_frames) * 2, 0);

    std::fprintf(stderr,
        "RetroRun hybrid audio: backend=sdl2, driver=%s, stable_buffer=%s, "
        "frequency=%d, samples=%d, target_queue_ms=%d, prefill_ms=%d, "
        "stretch_percent=%d, stretch_low_ms=%d\n",
        SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "unknown",
        retrorun_audio_stable_buffer ? "on" : "off",
        audio->frequency, audio->period_frames, audio->target_queue_ms,
        audio->prefill_ms, audio->stretch_percent, audio->stretch_low_ms);
    if (audio->started)
        SDL_PauseAudioDevice(audio->device, 0);
    return audio;
}

void destroy(void* handle) {
    sdl_audio* audio = static_cast<sdl_audio*>(handle);
    if (!audio)
        return;
    if (audio->device)
        SDL_CloseAudioDevice(audio->device);
    delete audio;
}

bool submit(void* handle, const short* data, int frames) {
    sdl_audio* audio = static_cast<sdl_audio*>(handle);
    if (!audio || !audio->device || !data || frames <= 0 ||
        audio->cancelled.load(std::memory_order_relaxed))
        return false;

    const size_t samples = static_cast<size_t>(frames) * 2;
    const Uint32 bytes_per_ms = static_cast<Uint32>(
        audio->frequency * 2 * sizeof(short) / 1000);
    const Uint32 target_queue = bytes_per_ms * audio->target_queue_ms;
    Uint32 queued = SDL_GetQueuedAudioSize(audio->device);
    const uint64_t queued_frames = queued / (2 * sizeof(short));
    uint64_t current_max = audio->max_queue_depth.load(std::memory_order_relaxed);
    while (current_max < queued_frames &&
           !audio->max_queue_depth.compare_exchange_weak(
               current_max, queued_frames, std::memory_order_relaxed)) {}
    audio->queue_depth_samples.fetch_add(1, std::memory_order_relaxed);
    audio->queue_depth_total_frames.fetch_add(queued_frames,
                                               std::memory_order_relaxed);
    if (audio->submitted_once) {
        uint64_t current_min = audio->min_queue_depth.load(std::memory_order_relaxed);
        while (current_min > queued_frames &&
               !audio->min_queue_depth.compare_exchange_weak(
                   current_min, queued_frames, std::memory_order_relaxed)) {}
        if (queued_frames == 0 && audio->started &&
            !audio->paused.load(std::memory_order_relaxed))
            audio->queue_empty_observations.fetch_add(1, std::memory_order_relaxed);
        if (queued_frames < static_cast<uint64_t>(audio->period_frames) &&
            audio->started &&
            !audio->paused.load(std::memory_order_relaxed))
            audio->queue_low_observations.fetch_add(1, std::memory_order_relaxed);
    }

    if (!retrorun_audio_stable_buffer && audio->prefill_frames > 0 &&
        audio->started && queued == 0 &&
        !audio->paused.load(std::memory_order_relaxed)) {
        audio->underruns.fetch_add(1, std::memory_order_relaxed);
    }

    if (retrorun_audio_stable_buffer && (!audio->started || queued == 0)) {
        if (audio->started && queued == 0 &&
            !audio->paused.load(std::memory_order_relaxed))
            audio->underruns.fetch_add(1, std::memory_order_relaxed);
        const int startup_periods = std::max(1,
            (audio->frequency * 40 / 1000 + audio->period_frames - 1) /
            audio->period_frames);
        const int periods = audio->started ? 1 : startup_periods;
        for (int i = 0; i < periods; ++i) {
            SDL_QueueAudio(audio->device, audio->silence_buffer.data(),
                static_cast<Uint32>(audio->silence_buffer.size() * sizeof(short)));
        }
        queued = SDL_GetQueuedAudioSize(audio->device);
    }

    bool pressure_counted = false;
    while (queued > target_queue &&
           !audio->cancelled.load(std::memory_order_relaxed)) {
        if (!pressure_counted) {
            audio->overruns.fetch_add(1, std::memory_order_relaxed);
            pressure_counted = true;
        }
        SDL_Delay(1);
        queued = SDL_GetQueuedAudioSize(audio->device);
    }
    if (audio->cancelled.load(std::memory_order_relaxed))
        return false;

    const short* queue_data = data;
    int queue_frames = frames;
    size_t queue_samples = samples;
    if (audio->stretch_percent > 0 && audio->started &&
        queued_frames < audio->stretch_low_frames &&
        !audio->paused.load(std::memory_order_relaxed)) {
        queue_frames = frames + std::max(1,
            frames * audio->stretch_percent / 100);
        queue_samples = static_cast<size_t>(queue_frames) * 2;
        audio->stretch_buffer.resize(queue_samples);
        const uint64_t phase_step = queue_frames > 1
            ? (static_cast<uint64_t>(frames - 1) << 16) /
                  static_cast<uint64_t>(queue_frames - 1)
            : 0;
        uint64_t phase = 0;
        for (int output_frame = 0; output_frame < queue_frames; ++output_frame) {
            const int source_frame = std::min<int>(phase >> 16, frames - 1);
            const int next_frame = std::min(source_frame + 1, frames - 1);
            const int fraction = static_cast<int>(phase & 0xffff);
            for (int channel = 0; channel < 2; ++channel) {
                const int first = data[source_frame * 2 + channel];
                const int second = data[next_frame * 2 + channel];
                audio->stretch_buffer[output_frame * 2 + channel] =
                    static_cast<short>(first +
                        ((static_cast<int64_t>(second - first) * fraction) >> 16));
            }
            phase += phase_step;
        }
        queue_data = audio->stretch_buffer.data();
        audio->adaptive_stretch_frames.fetch_add(
            static_cast<uint64_t>(queue_frames - frames),
            std::memory_order_relaxed);
    }

    const int volume = audio->volume.load(std::memory_order_relaxed);
    int result = 0;
    if (volume >= 100) {
        result = SDL_QueueAudio(audio->device, queue_data,
            static_cast<Uint32>(queue_samples * sizeof(short)));
    } else {
        audio->mix_buffer.resize(queue_samples);
        for (size_t i = 0; i < queue_samples; ++i)
            audio->mix_buffer[i] = static_cast<short>((queue_data[i] * volume) / 100);
        result = SDL_QueueAudio(audio->device, audio->mix_buffer.data(),
            static_cast<Uint32>(audio->mix_buffer.size() * sizeof(short)));
    }
    if (result != 0) {
        audio->frames_dropped.fetch_add(static_cast<uint64_t>(frames),
                                        std::memory_order_relaxed);
        return false;
    }
    audio->submitted_once = true;

    const uint64_t queued_after = SDL_GetQueuedAudioSize(audio->device) /
        (2 * sizeof(short));
    if (retrorun_audio_stable_buffer && !audio->started) {
        audio->started = true;
        SDL_PauseAudioDevice(audio->device, 0);
    } else if (!retrorun_audio_stable_buffer && audio->prefill_frames > 0 &&
               !audio->started && queued_after >= audio->prefill_frames &&
               !audio->paused.load(std::memory_order_relaxed)) {
        audio->started = true;
        SDL_PauseAudioDevice(audio->device, 0);
    }
    return true;
}

void release_thread(void*) {}
bool valid(void* handle) {
    const sdl_audio* audio = static_cast<const sdl_audio*>(handle);
    return audio && audio->device != 0;
}
void flush(void* handle) {
    sdl_audio* audio = static_cast<sdl_audio*>(handle);
    if (!audio || !audio->device)
        return;
    SDL_ClearQueuedAudio(audio->device);
    audio->started = !retrorun_audio_stable_buffer && audio->prefill_frames == 0;
    audio->submitted_once = false;
    if (!audio->started)
        SDL_PauseAudioDevice(audio->device, 1);
}
void pause(void* handle, bool paused) {
    sdl_audio* audio = static_cast<sdl_audio*>(handle);
    if (!audio || !audio->device)
        return;
    audio->paused.store(paused, std::memory_order_relaxed);
    if (paused) {
        SDL_PauseAudioDevice(audio->device, 1);
    } else if (audio->started) {
        SDL_PauseAudioDevice(audio->device, 0);
    }
}
void cancel(void* handle) {
    sdl_audio* audio = static_cast<sdl_audio*>(handle);
    if (audio)
        audio->cancelled.store(true, std::memory_order_relaxed);
}
void diagnostics_get(void* handle, rr_audio_diagnostics_t* diagnostics) {
    if (!diagnostics)
        return;
    *diagnostics = {};
    const sdl_audio* audio = static_cast<const sdl_audio*>(handle);
    if (!audio)
        return;
    diagnostics->buffer_underruns = audio->underruns.load();
    diagnostics->buffer_overruns = audio->overruns.load();
    diagnostics->frames_dropped = audio->frames_dropped.load();
    diagnostics->max_queue_depth = audio->max_queue_depth.load();
    const uint64_t min_depth = audio->min_queue_depth.load();
    diagnostics->min_queue_depth =
        min_depth == std::numeric_limits<uint64_t>::max() ? 0 : min_depth;
    diagnostics->queue_depth_samples = audio->queue_depth_samples.load();
    diagnostics->queue_depth_total_frames = audio->queue_depth_total_frames.load();
    diagnostics->queue_empty_observations = audio->queue_empty_observations.load();
    diagnostics->queue_low_observations = audio->queue_low_observations.load();
    diagnostics->adaptive_stretch_frames = audio->adaptive_stretch_frames.load();
}
uint32_t volume_get(void* handle, const char*) {
    const sdl_audio* audio = static_cast<const sdl_audio*>(handle);
    return audio ? static_cast<uint32_t>(audio->volume.load()) : 0;
}
void volume_set(void* handle, uint32_t value, const char*) {
    sdl_audio* audio = static_cast<sdl_audio*>(handle);
    if (audio)
        audio->volume.store(static_cast<int>(std::min<uint32_t>(value, 100)),
                            std::memory_order_relaxed);
}

const rr_audio_backend_api api = {
    "sdl2",
    create,
    destroy,
    submit,
    release_thread,
    valid,
    flush,
    pause,
    cancel,
    diagnostics_get,
    volume_get,
    volume_set,
};

} // namespace

const rr_audio_backend_api* rr_audio_backend_sdl2() { return &api; }
