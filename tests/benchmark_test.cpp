#include "benchmark.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>

using Clock = std::chrono::steady_clock;

static std::string read_file(const char* path)
{
    std::ifstream input(path);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

static void run_counter_snapshot(const char* path, unsigned presentations)
{
    BenchmarkOptions options;
    options.duration_seconds = 0.015;
    options.warmup_seconds = 0.0;
    options.json_path = path;
    std::string error;
    assert(benchmark_configure(options, &error));

    BenchmarkMetadata metadata;
    metadata.release = "test";
    metadata.device = "fake";
    metadata.backend = "fake";
    metadata.audio_backend = "fake-audio";
    metadata.renderer = "fake-presenter";
    metadata.core_name = "fake-core";
    metadata.core_version = "1";
    metadata.declared_fps = 60.0;
    metadata.sample_rate = 48000.0;
    metadata.audio_buffer = 512;
    metadata.sdl_audio_stretch_percent = 5;
    metadata.sdl_audio_stretch_low_ms = 40;
    metadata.go2_audio_stretch_percent = 4;
    metadata.go2_audio_stretch_low_ms = 35;
    metadata.confirm_input = true;
    metadata.confirm_input_delay_seconds = 4.0;
    benchmark_set_metadata(metadata);
    benchmark_begin_warmup();
    assert(benchmark_collecting());

    uint64_t generation = 0;
    unsigned frame = 0;
    while (!benchmark_deadline_reached()) {
        benchmark_frame_begin();
        benchmark_core_begin();
        benchmark_video_callback_begin();
        benchmark_video_callback_end();
        benchmark_audio_callback_begin();
        benchmark_audio_callback_end(4);
        benchmark_core_end();
        if (frame < presentations) {
            generation = benchmark_capture_generation();
            benchmark_presentation_completed(
                frame % 2 ? BenchmarkPresentation::Direct
                          : BenchmarkPresentation::Fallback,
                10, generation);
        }
        benchmark_frame_end(Clock::now() + std::chrono::milliseconds(1), true);
        ++frame;
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    // A frame accepted inside the measurement remains attributable while the
    // frontend drains asynchronous presenter work after the timer closes.
    if (generation)
        benchmark_presentation_completed(BenchmarkPresentation::Fallback, 20,
                                         generation);

    BenchmarkAudioDiagnostics audio;
    audio.max_queue_depth = 32;
    audio.min_queue_depth = 4;
    audio.queue_empty_observations = 3;
    audio.adaptive_stretch_frames = 17;
    audio.producer_late_over_10ms = 5;
    audio.backend_submit_max_duration_us = 12000;
    audio.backpressure_events = 2;
    benchmark_set_audio_diagnostics(audio);
    assert(benchmark_finish_and_report());

    const std::string json = read_file(path);
    assert(json.find("\"release\":\"test\"") != std::string::npos);
    assert(json.find("\"audio_backend\":\"fake-audio\"") != std::string::npos);
    assert(json.find("\"renderer\":\"fake-presenter\"") != std::string::npos);
    assert(json.find("\"audio_buffer\":512") != std::string::npos);
    assert(json.find("\"sdl_audio_stretch_percent\":5") != std::string::npos);
    assert(json.find("\"sdl_audio_stretch_low_ms\":40") != std::string::npos);
    assert(json.find("\"go2_audio_stretch_percent\":4") != std::string::npos);
    assert(json.find("\"go2_audio_stretch_low_ms\":35") != std::string::npos);
    assert(json.find("\"confirm_input\":true") != std::string::npos);
    assert(json.find("\"confirm_input_delay_seconds\":4.000") != std::string::npos);
    assert(json.find("\"core_p95\"") != std::string::npos);
    assert(json.find("\"active_frame_p99\"") != std::string::npos);
    assert(json.find("\"backpressure_events\":2") != std::string::npos);
    assert(json.find("\"max_queue_depth\":32") != std::string::npos);
    assert(json.find("\"min_queue_depth\":4") != std::string::npos);
    assert(json.find("\"queue_empty_observations\":3") != std::string::npos);
    assert(json.find("\"adaptive_stretch_frames\":17") != std::string::npos);
    assert(json.find("\"producer_late_over_10ms\":5") != std::string::npos);
    assert(json.find("\"backend_submit_max_duration_us\":12000") != std::string::npos);
}

int main()
{
    BenchmarkOptions invalid;
    invalid.duration_seconds = 0.0;
    std::string error;
    assert(!benchmark_configure(invalid, &error));
    assert(!error.empty());

    const char* first = "benchmark-result-1.json";
    const char* second = "benchmark-result-2.json";
    run_counter_snapshot(first, 4);
    const std::string first_json = read_file(first);
    run_counter_snapshot(second, 1);
    const std::string second_json = read_file(second);

    // A fresh run must reset counters instead of accumulating the first run.
    assert(first_json.find("\"presented_frames\":5") != std::string::npos);
    assert(second_json.find("\"presented_frames\":2") != std::string::npos);

    std::remove(first);
    std::remove(second);
    return 0;
}
