#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

enum class BenchmarkSkipReason {
    Fixed,
    Adaptive,
    FastForward,
    PresenterQueue
};

enum class BenchmarkPresentation {
    Direct,
    Fallback
};

struct BenchmarkOptions {
    double duration_seconds = 0.0;
    double warmup_seconds = 0.0;
    uint64_t core_frames = 0;
    std::string json_path;
};

struct BenchmarkMetadata {
    std::string release;
    std::string device;
    std::string backend;
    std::string audio_backend;
    std::string renderer;
    std::string core_name;
    std::string core_version;
    double declared_fps = 0.0;
    double sample_rate = 0.0;
    bool declared_fps_pacing = false;
    bool threaded_audio = false;
    bool stable_audio_buffer = false;
    int audio_buffer = -1;
    int sdl_audio_stretch_percent = 0;
    int sdl_audio_stretch_low_ms = 40;
    int go2_audio_stretch_percent = 0;
    int go2_audio_stretch_low_ms = 40;
    int go2_audio_prebuffer_ms = 60;
    bool threaded_video = false;
    bool direct_scanout = false;
    bool overlays = false;
    bool vsync_requested = false;
    bool vsync_applied = false;
    int fixed_frameskip = 0;
    bool adaptive_frameskip = false;
    bool confirm_input = false;
    double confirm_input_delay_seconds = 4.0;
};

enum class BenchmarkConfirmButton {
    A,
    B
};

struct BenchmarkAudioDiagnostics {
    uint64_t queued_frames = 0;
    uint64_t in_flight_frames = 0;
    uint64_t buffer_underruns = 0;
    uint64_t buffer_overruns = 0;
    uint64_t frames_dropped = 0;
    uint64_t intentional_muted_frames = 0;
    uint64_t flush_discarded_frames = 0;
    uint64_t backpressure_events = 0;
    uint64_t max_queue_depth = 0;
    uint64_t min_queue_depth = 0;
    uint64_t queue_depth_samples = 0;
    uint64_t queue_depth_total_frames = 0;
    uint64_t queue_empty_observations = 0;
    uint64_t queue_low_observations = 0;
    uint64_t adaptive_stretch_frames = 0;
    uint64_t callback_max_duration_us = 0;
    uint64_t producer_gap_max_us = 0;
    uint64_t producer_late_max_us = 0;
    uint64_t producer_late_over_1ms = 0;
    uint64_t producer_late_over_5ms = 0;
    uint64_t producer_late_over_10ms = 0;
    uint64_t producer_late_over_20ms = 0;
    uint64_t backend_submit_time_us = 0;
    uint64_t backend_submit_max_duration_us = 0;
    uint64_t backend_submit_over_1ms = 0;
    uint64_t backend_submit_over_5ms = 0;
    uint64_t backend_submit_over_10ms = 0;
    uint64_t backend_submit_over_20ms = 0;
};

extern std::atomic<bool> g_benchmark_collecting;

inline bool benchmark_collecting()
{
    return g_benchmark_collecting.load(std::memory_order_relaxed);
}

bool benchmark_requested();
bool benchmark_configure(const BenchmarkOptions& options, std::string* error);
void benchmark_set_metadata(const BenchmarkMetadata& metadata);
void benchmark_update_av(double declared_fps, double sample_rate);
void benchmark_begin_warmup();
void benchmark_set_confirm_input(bool enabled);
void benchmark_set_confirm_input_delay(double seconds);
bool benchmark_confirm_input_enabled();
double benchmark_confirm_input_delay();
bool benchmark_confirm_button_pressed(BenchmarkConfirmButton button);
bool benchmark_update_window();
bool benchmark_deadline_reached();
void benchmark_abort(const std::string& reason);

void benchmark_frame_begin();
void benchmark_core_begin();
void benchmark_core_end();
void benchmark_frame_end(std::chrono::steady_clock::time_point deadline,
                         bool paced_frame);

void benchmark_video_callback_begin();
void benchmark_video_callback_end();
void benchmark_audio_callback_begin();
void benchmark_audio_callback_end(uint64_t frames);

void benchmark_video_duplicate();
void benchmark_video_skipped(BenchmarkSkipReason reason);
uint64_t benchmark_capture_generation();
void benchmark_presentation_completed(BenchmarkPresentation kind,
                                      uint64_t backend_time_us = 0,
                                      uint64_t generation = 0);
void benchmark_presentation_rejected();
void benchmark_set_audio_diagnostics(const BenchmarkAudioDiagnostics& diagnostics);

// Call only after audio/video producers and presenters have been drained.
// Returns false when no valid measurement was completed.
bool benchmark_finish_and_report();
