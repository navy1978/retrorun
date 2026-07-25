#include "audio_backend.h"

#include "go2/audio.h"

namespace {

void* create(int frequency) { return go2_audio_create(frequency); }
void destroy(void* audio) { go2_audio_destroy(static_cast<go2_audio_t*>(audio)); }
bool submit(void* audio, const short* data, int frames) {
    return go2_audio_submit(static_cast<go2_audio_t*>(audio), data, frames);
}
void release_thread(void* audio) {
    go2_audio_release_thread(static_cast<go2_audio_t*>(audio));
}
bool valid(void* audio) { return go2_audio_valid(static_cast<go2_audio_t*>(audio)); }
void flush(void* audio) { go2_audio_flush(static_cast<go2_audio_t*>(audio)); }
void pause(void* audio, bool paused) {
    go2_audio_pause(static_cast<go2_audio_t*>(audio), paused);
}
void cancel(void* audio) { go2_audio_cancel(static_cast<go2_audio_t*>(audio)); }
void diagnostics_get(void* audio, rr_audio_diagnostics_t* diagnostics) {
    if (!diagnostics)
        return;
    *diagnostics = {};
    go2_audio_diagnostics_get(static_cast<go2_audio_t*>(audio),
        &diagnostics->buffer_underruns, &diagnostics->buffer_overruns,
        &diagnostics->frames_dropped, &diagnostics->max_queue_depth,
        &diagnostics->min_queue_depth, &diagnostics->queue_depth_samples,
        &diagnostics->queue_depth_total_frames,
        &diagnostics->queue_empty_observations,
        &diagnostics->queue_low_observations,
        &diagnostics->adaptive_stretch_frames);
}
uint32_t volume_get(void* audio, const char* control) {
    return go2_audio_volume_get(static_cast<go2_audio_t*>(audio), control);
}
void volume_set(void* audio, uint32_t value, const char* control) {
    go2_audio_volume_set(static_cast<go2_audio_t*>(audio), value, control);
}

const rr_audio_backend_api api = {
    "go2",
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

const rr_audio_backend_api* rr_audio_backend_go2() { return &api; }
