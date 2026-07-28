/*
retrorun - lightweight cross-platform libretro frontend
Copyright (C) 2020 OtherCrashOverride
Copyright (C) 2021-present navy1978
*/

#include "audio.h"
#include "benchmark.h"
#include "core_loader.h"
#include "globals.h"
#include "input.h"
#include "platform.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

extern int opt_volume;

namespace {

constexpr size_t CHANNELS = 2;

enum class AudioCommandType { Data, Flush, Pause, Resume, Stop };
enum class AudioEngineState { Stopped, Running, Paused, Stopping };

struct AudioCommand {
    AudioCommandType type = AudioCommandType::Data;
    uint64_t id = 0;
    std::vector<short> samples;
    size_t frames = 0;
};

struct AudioEngine {
    rr_audio_t* backend = nullptr;
    int frequency = 0;
    size_t target_frames = 1;
    size_t capacity_frames = 0;
    std::vector<short> staging;
    // Libretro normally invokes audio callbacks from retro_run(), but cores
    // such as Flycast may produce audio from an internal worker. Keep the
    // staging vector and the per-callback buffer threshold single-owner even
    // when the core calls the frontend concurrently.
    std::mutex callback_mutex;

    std::mutex mutex;
    std::condition_variable ready;
    std::condition_variable space;
    std::condition_variable acknowledged;
    std::deque<AudioCommand> commands;
    std::thread worker;
    AudioEngineState state = AudioEngineState::Stopped;
    size_t queued_frames = 0;
    size_t in_flight_frames = 0;
    uint64_t next_command_id = 1;
    uint64_t acknowledged_id = 0;

    std::atomic<uint64_t> buffer_underruns{0};
    std::atomic<uint64_t> buffer_overruns{0};
    std::atomic<uint64_t> frames_dropped{0};
    std::atomic<uint64_t> intentional_muted_frames{0};
    std::atomic<uint64_t> flush_discarded_frames{0};
    std::atomic<uint64_t> backpressure_events{0};
    std::atomic<uint64_t> max_queue_depth{0};
    std::atomic<uint64_t> callback_max_duration_us{0};
    std::atomic<uint64_t> producer_gap_max_us{0};
    std::atomic<uint64_t> producer_late_max_us{0};
    std::atomic<uint64_t> producer_late_over_1ms{0};
    std::atomic<uint64_t> producer_late_over_5ms{0};
    std::atomic<uint64_t> producer_late_over_10ms{0};
    std::atomic<uint64_t> producer_late_over_20ms{0};
    std::atomic<uint64_t> last_callback_ns{0};
    std::atomic<uint64_t> last_callback_frames{0};
    std::atomic<uint64_t> backend_submit_time_us{0};
    std::atomic<uint64_t> backend_submit_max_duration_us{0};
    std::atomic<uint64_t> backend_submit_over_1ms{0};
    std::atomic<uint64_t> backend_submit_over_5ms{0};
    std::atomic<uint64_t> backend_submit_over_10ms{0};
    std::atomic<uint64_t> backend_submit_over_20ms{0};
};

AudioEngine engine;
std::atomic<uint64_t> fastForwardAudioDroppedFrames{0};
int previousVolume = -1;
std::string soundCardName;

void update_max(std::atomic<uint64_t>& target, uint64_t value)
{
    uint64_t current = target.load(std::memory_order_relaxed);
    while (current < value &&
           !target.compare_exchange_weak(current, value, std::memory_order_relaxed)) {}
}

RetrorunAudioQueueSnapshot audio_queue_snapshot_internal()
{
    uint64_t queued_frames = 0;
    uint64_t capacity_frames = 0;
    {
        std::lock_guard<std::mutex> lock(engine.mutex);
        queued_frames = engine.queued_frames + engine.in_flight_frames;
        capacity_frames = engine.capacity_frames;
    }

    {
        std::lock_guard<std::mutex> lock(engine.callback_mutex);
        const uint64_t staged_frames =
            engine.staging.size() / CHANNELS;
        queued_frames += staged_frames;
    }

    if (queued_frames > UINT32_MAX)
        queued_frames = UINT32_MAX;
    if (capacity_frames < queued_frames)
        capacity_frames = queued_frames;

    return {static_cast<uint32_t>(queued_frames),
            static_cast<uint32_t>(capacity_frames)};
}

void record_thresholds(uint64_t value_us,
                       std::atomic<uint64_t>& over_1ms,
                       std::atomic<uint64_t>& over_5ms,
                       std::atomic<uint64_t>& over_10ms,
                       std::atomic<uint64_t>& over_20ms)
{
    if (value_us >= 1000) over_1ms.fetch_add(1, std::memory_order_relaxed);
    if (value_us >= 5000) over_5ms.fetch_add(1, std::memory_order_relaxed);
    if (value_us >= 10000) over_10ms.fetch_add(1, std::memory_order_relaxed);
    if (value_us >= 20000) over_20ms.fetch_add(1, std::memory_order_relaxed);
}

void record_producer_gap(uint64_t frames,
                         std::chrono::steady_clock::time_point now)
{
    const uint64_t now_ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count());
    const uint64_t previous_ns = engine.last_callback_ns.exchange(
        now_ns, std::memory_order_relaxed);
    const uint64_t previous_frames = engine.last_callback_frames.exchange(
        frames, std::memory_order_relaxed);
    if (previous_ns == 0 || now_ns <= previous_ns || engine.frequency <= 0)
        return;

    const uint64_t interval_us = (now_ns - previous_ns) / 1000;
    const uint64_t expected_us = previous_frames * 1000000ULL /
                                 static_cast<uint64_t>(engine.frequency);
    const uint64_t late_us = interval_us > expected_us
        ? interval_us - expected_us : 0;
    update_max(engine.producer_gap_max_us, interval_us);
    update_max(engine.producer_late_max_us, late_us);
    record_thresholds(late_us,
                      engine.producer_late_over_1ms,
                      engine.producer_late_over_5ms,
                      engine.producer_late_over_10ms,
                      engine.producer_late_over_20ms);
}

void record_backend_submit(std::chrono::steady_clock::time_point started)
{
    if (started.time_since_epoch().count() == 0)
        return;
    const uint64_t elapsed = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count());
    engine.backend_submit_time_us.fetch_add(elapsed, std::memory_order_relaxed);
    update_max(engine.backend_submit_max_duration_us, elapsed);
    record_thresholds(elapsed,
                      engine.backend_submit_over_1ms,
                      engine.backend_submit_over_5ms,
                      engine.backend_submit_over_10ms,
                      engine.backend_submit_over_20ms);
}

struct AudioCallbackScope {
    bool measured;
    uint64_t frames;
    std::chrono::steady_clock::time_point started;
    AudioCallbackScope(uint64_t frame_count)
        : measured(benchmark_collecting()), frames(frame_count),
          started(measured ? std::chrono::steady_clock::now()
                           : std::chrono::steady_clock::time_point{})
    {
        if (measured) {
            benchmark_audio_callback_begin();
            record_producer_gap(frames, started);
        }
    }
    ~AudioCallbackScope()
    {
        if (!measured) return;
        const uint64_t elapsed = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - started).count());
        update_max(engine.callback_max_duration_us, elapsed);
        benchmark_audio_callback_end(frames);
    }
};

void acknowledge(uint64_t id)
{
    std::lock_guard<std::mutex> lock(engine.mutex);
    engine.acknowledged_id = std::max(engine.acknowledged_id, id);
    engine.acknowledged.notify_all();
}

void audio_worker_loop()
{
    for (;;) {
        AudioCommand command;
        {
            std::unique_lock<std::mutex> lock(engine.mutex);
            engine.ready.wait(lock, [] { return !engine.commands.empty(); });
            command = std::move(engine.commands.front());
            engine.commands.pop_front();
            if (command.type == AudioCommandType::Data) {
                engine.queued_frames -= command.frames;
                engine.in_flight_frames = command.frames;
            }
        }
        engine.space.notify_all();

        if (command.type == AudioCommandType::Data) {
            const auto started = benchmark_collecting()
                ? std::chrono::steady_clock::now()
                : std::chrono::steady_clock::time_point{};
            const bool accepted = rr_audio_submit(engine.backend, command.samples.data(),
                                                  static_cast<int>(command.frames));
            if (!accepted)
                engine.frames_dropped.fetch_add(command.frames, std::memory_order_relaxed);
            record_backend_submit(started);
            {
                std::lock_guard<std::mutex> lock(engine.mutex);
                engine.in_flight_frames = 0;
            }
            engine.space.notify_all();
            continue;
        }

        if (command.type == AudioCommandType::Flush) {
            rr_audio_flush(engine.backend);
            acknowledge(command.id);
            continue;
        }
        if (command.type == AudioCommandType::Pause) {
            rr_audio_pause(engine.backend, true);
            {
                std::lock_guard<std::mutex> lock(engine.mutex);
                engine.state = AudioEngineState::Paused;
            }
            acknowledge(command.id);
            continue;
        }
        if (command.type == AudioCommandType::Resume) {
            rr_audio_pause(engine.backend, false);
            {
                std::lock_guard<std::mutex> lock(engine.mutex);
                engine.state = AudioEngineState::Running;
            }
            acknowledge(command.id);
            continue;
        }

        rr_audio_release_thread(engine.backend);
        acknowledge(command.id);
        return;
    }
}

uint64_t enqueue_control(AudioCommandType type, bool front = false)
{
    std::lock_guard<std::mutex> lock(engine.mutex);
    AudioCommand command;
    command.type = type;
    command.id = engine.next_command_id++;
    const uint64_t id = command.id;
    if (front) engine.commands.push_front(std::move(command));
    else engine.commands.push_back(std::move(command));
    engine.ready.notify_one();
    return id;
}

bool wait_ack(uint64_t id)
{
    std::unique_lock<std::mutex> lock(engine.mutex);
    engine.acknowledged.wait(lock, [id] { return engine.acknowledged_id >= id; });
    return true;
}

void record_depth_locked()
{
    update_max(engine.max_queue_depth,
               static_cast<uint64_t>(engine.queued_frames + engine.in_flight_frames));
}

void submit_chunk(const short* samples, size_t frames)
{
    if (!engine.backend || frames == 0) return;

    if (!forceAudioMultithread) {
        const auto started = benchmark_collecting()
            ? std::chrono::steady_clock::now()
            : std::chrono::steady_clock::time_point{};
        if (!rr_audio_submit(engine.backend, samples, static_cast<int>(frames)))
            engine.frames_dropped.fetch_add(frames, std::memory_order_relaxed);
        record_backend_submit(started);
        return;
    }

    size_t offset = 0;
    while (offset < frames) {
        const size_t part = std::min(engine.capacity_frames, frames - offset);
        AudioCommand command;
        command.type = AudioCommandType::Data;
        command.frames = part;
        command.samples.assign(samples + offset * CHANNELS,
                               samples + (offset + part) * CHANNELS);
        std::unique_lock<std::mutex> lock(engine.mutex);
        if (engine.queued_frames + engine.in_flight_frames + part > engine.capacity_frames)
            engine.backpressure_events.fetch_add(1, std::memory_order_relaxed);
        engine.space.wait(lock, [part] {
            return engine.state == AudioEngineState::Stopping ||
                   engine.queued_frames + engine.in_flight_frames + part <= engine.capacity_frames;
        });
        if (engine.state == AudioEngineState::Stopping) {
            engine.frames_dropped.fetch_add(frames - offset, std::memory_order_relaxed);
            return;
        }
        engine.queued_frames += part;
        engine.commands.push_back(std::move(command));
        record_depth_locked();
        lock.unlock();
        engine.ready.notify_one();
        offset += part;
    }
}

void flush_staging_chunks()
{
    const size_t target = std::max<size_t>(1, engine.target_frames);
    while (engine.staging.size() / CHANNELS >= target) {
        submit_chunk(engine.staging.data(), target);
        engine.staging.erase(engine.staging.begin(),
                             engine.staging.begin() + target * CHANNELS);
    }
}

void update_volume()
{
    if (!engine.backend || opt_volume == previousVolume) return;
    rr_audio_volume_set(engine.backend, static_cast<uint32_t>(opt_volume), soundCardName.c_str());
    previousVolume = opt_volume;
}

BenchmarkAudioDiagnostics snapshot_diagnostics()
{
    rr_audio_diagnostics_t backend{};
    rr_audio_diagnostics_get(engine.backend, &backend);
    BenchmarkAudioDiagnostics result;
    {
        std::lock_guard<std::mutex> lock(engine.mutex);
        result.queued_frames = engine.queued_frames;
        result.in_flight_frames = engine.in_flight_frames;
    }
    result.buffer_underruns = engine.buffer_underruns.load() + backend.buffer_underruns;
    result.buffer_overruns = engine.buffer_overruns.load() + backend.buffer_overruns;
    result.frames_dropped = engine.frames_dropped.load() + backend.frames_dropped;
    result.intentional_muted_frames = engine.intentional_muted_frames.load();
    result.flush_discarded_frames = engine.flush_discarded_frames.load();
    result.backpressure_events = engine.backpressure_events.load();
    result.max_queue_depth = std::max(engine.max_queue_depth.load(), backend.max_queue_depth);
    result.min_queue_depth = backend.min_queue_depth;
    result.queue_depth_samples = backend.queue_depth_samples;
    result.queue_depth_total_frames = backend.queue_depth_total_frames;
    result.queue_empty_observations = backend.queue_empty_observations;
    result.queue_low_observations = backend.queue_low_observations;
    result.adaptive_stretch_frames = backend.adaptive_stretch_frames;
    result.callback_max_duration_us = engine.callback_max_duration_us.load();
    result.producer_gap_max_us = engine.producer_gap_max_us.load();
    result.producer_late_max_us = engine.producer_late_max_us.load();
    result.producer_late_over_1ms = engine.producer_late_over_1ms.load();
    result.producer_late_over_5ms = engine.producer_late_over_5ms.load();
    result.producer_late_over_10ms = engine.producer_late_over_10ms.load();
    result.producer_late_over_20ms = engine.producer_late_over_20ms.load();
    result.backend_submit_time_us = engine.backend_submit_time_us.load();
    result.backend_submit_max_duration_us = engine.backend_submit_max_duration_us.load();
    result.backend_submit_over_1ms = engine.backend_submit_over_1ms.load();
    result.backend_submit_over_5ms = engine.backend_submit_over_5ms.load();
    result.backend_submit_over_10ms = engine.backend_submit_over_10ms.load();
    result.backend_submit_over_20ms = engine.backend_submit_over_20ms.load();
    return result;
}

} // namespace

RetrorunAudioQueueSnapshot audio_queue_snapshot()
{
    return audio_queue_snapshot_internal();
}

void audio_init(int frequency, double fps)
{
    engine.backend = rr_audio_create(frequency);
    if (!engine.backend || !rr_audio_valid(engine.backend))
        throw std::runtime_error("audio backend initialization failed");
    engine.frequency = frequency;
    engine.target_frames = static_cast<size_t>(std::max(1.0, frequency / std::max(1.0, fps)));
    if (retrorun_audio_buffer < 1)
        retrorun_audio_buffer = static_cast<int>(engine.target_frames);
    else
        engine.target_frames = static_cast<size_t>(retrorun_audio_buffer);
    engine.capacity_frames = std::max<size_t>(2048, static_cast<size_t>(frequency) / 15);
    engine.staging.clear();
    engine.commands.clear();
    engine.queued_frames = 0;
    engine.in_flight_frames = 0;
    engine.state = AudioEngineState::Running;
    engine.acknowledged_id = 0;
    engine.next_command_id = 1;

    engine.buffer_underruns = 0;
    engine.buffer_overruns = 0;
    engine.frames_dropped = 0;
    engine.intentional_muted_frames = 0;
    engine.flush_discarded_frames = 0;
    engine.backpressure_events = 0;
    engine.max_queue_depth = 0;
    engine.callback_max_duration_us = 0;
    engine.producer_gap_max_us = 0;
    engine.producer_late_max_us = 0;
    engine.producer_late_over_1ms = 0;
    engine.producer_late_over_5ms = 0;
    engine.producer_late_over_10ms = 0;
    engine.producer_late_over_20ms = 0;
    engine.last_callback_ns = 0;
    engine.last_callback_frames = 0;
    engine.backend_submit_time_us = 0;
    engine.backend_submit_max_duration_us = 0;
    engine.backend_submit_over_1ms = 0;
    engine.backend_submit_over_5ms = 0;
    engine.backend_submit_over_10ms = 0;
    engine.backend_submit_over_20ms = 0;

    if (forceAudioMultithread) {
        engine.worker = std::thread(audio_worker_loop);
        logger.log(Logger::INF, "Threaded audio enabled: capacity=%zu frames (%.1f ms)",
                   engine.capacity_frames,
                   frequency > 0 ? engine.capacity_frames * 1000.0 / frequency : 0.0);
    }

    soundCardName = isRG552() ? "DAC" : isRK3566Device() ? "Master" : "Playback";
    if (opt_volume > -1)
        rr_audio_volume_set(engine.backend, static_cast<uint32_t>(opt_volume), soundCardName.c_str());
    else
        opt_volume = static_cast<int>(rr_audio_volume_get(engine.backend, soundCardName.c_str()));
    previousVolume = opt_volume;
}

bool audio_reconfigure(int frequency, double fps)
{
    if (frequency <= 0 || !std::isfinite(fps) || fps <= 0.0)
        return false;
    if (engine.backend && engine.frequency == frequency) {
        audio_flush();
        engine.target_frames = static_cast<size_t>(std::max(1.0, frequency / fps));
        if (retrorun_audio_buffer > 0)
            engine.target_frames = static_cast<size_t>(retrorun_audio_buffer);
        return true;
    }
    audio_deinit();
    audio_init(frequency, fps);
    return true;
}

bool audio_flush()
{
    size_t staged = 0;
    {
        std::lock_guard<std::mutex> callback_lock(engine.callback_mutex);
        staged = engine.staging.size() / CHANNELS;
        engine.staging.clear();
    }
    engine.flush_discarded_frames.fetch_add(staged, std::memory_order_relaxed);
    if (!engine.backend) return true;
    if (!forceAudioMultithread) {
        rr_audio_flush(engine.backend);
        return true;
    }

    {
        std::lock_guard<std::mutex> lock(engine.mutex);
        for (auto it = engine.commands.begin(); it != engine.commands.end();) {
            if (it->type == AudioCommandType::Data) {
                engine.queued_frames -= it->frames;
                engine.flush_discarded_frames.fetch_add(it->frames, std::memory_order_relaxed);
                it = engine.commands.erase(it);
            } else {
                ++it;
            }
        }
    }
    engine.space.notify_all();
    return wait_ack(enqueue_control(AudioCommandType::Flush));
}

bool audio_pause()
{
    if (!engine.backend) return true;
    audio_flush();
    if (!forceAudioMultithread) {
        rr_audio_pause(engine.backend, true);
        engine.state = AudioEngineState::Paused;
        return true;
    }
    return wait_ack(enqueue_control(AudioCommandType::Pause));
}

bool audio_resume()
{
    if (!engine.backend) return true;
    if (!forceAudioMultithread) {
        rr_audio_pause(engine.backend, false);
        engine.state = AudioEngineState::Running;
        return true;
    }
    return wait_ack(enqueue_control(AudioCommandType::Resume));
}

void audio_discard_pending() { (void)audio_flush(); }

void audio_deinit()
{
    if (!engine.backend) return;
    size_t staged = 0;
    {
        std::lock_guard<std::mutex> callback_lock(engine.callback_mutex);
        staged = engine.staging.size() / CHANNELS;
        engine.staging.clear();
    }
    engine.flush_discarded_frames.fetch_add(staged, std::memory_order_relaxed);
    if (forceAudioMultithread && engine.worker.joinable()) {
        rr_audio_cancel(engine.backend);
        {
            std::lock_guard<std::mutex> lock(engine.mutex);
            engine.state = AudioEngineState::Stopping;
            for (const auto& command : engine.commands)
                if (command.type == AudioCommandType::Data)
                    engine.flush_discarded_frames.fetch_add(command.frames, std::memory_order_relaxed);
            engine.commands.clear();
            engine.queued_frames = 0;
        }
        engine.space.notify_all();
        const uint64_t stop = enqueue_control(AudioCommandType::Stop, true);
        wait_ack(stop);
        engine.worker.join();
    } else {
        rr_audio_cancel(engine.backend);
    }
    benchmark_set_audio_diagnostics(snapshot_diagnostics());
    rr_audio_destroy(engine.backend);
    engine.backend = nullptr;
    engine.staging.clear();
    engine.state = AudioEngineState::Stopped;
}

uint64_t fastForwardAudioFramesDropped()
{
    return fastForwardAudioDroppedFrames.exchange(0, std::memory_order_relaxed);
}

void setVolume(int value)
{
    opt_volume = value;
    update_volume();
}

int getVolume()
{
    return engine.backend
        ? static_cast<int>(rr_audio_volume_get(engine.backend, soundCardName.c_str())) : 0;
}

void core_audio_sample(int16_t left, int16_t right)
{
    if (!core_callbacks_enabled()) return;
    AudioCallbackScope scope(1);
    if (input_ffwd_requested || audio_disabled) {
        if (input_ffwd_requested) {
            fastForwardAudioDroppedFrames.fetch_add(1, std::memory_order_relaxed);
            engine.intentional_muted_frames.fetch_add(1, std::memory_order_relaxed);
        }
        return;
    }
    std::lock_guard<std::mutex> callback_lock(engine.callback_mutex);
    if (!core_callbacks_enabled() || !engine.backend)
        return;
    update_volume();
    engine.staging.push_back(left);
    engine.staging.push_back(right);
    flush_staging_chunks();
}

size_t core_audio_sample_batch(const int16_t* data, size_t frames)
{
    if (!core_callbacks_enabled()) return frames;
    AudioCallbackScope scope(frames);
    if (!data || frames == 0) return frames;
    if (input_ffwd_requested || audio_disabled) {
        if (input_ffwd_requested) {
            fastForwardAudioDroppedFrames.fetch_add(frames, std::memory_order_relaxed);
            engine.intentional_muted_frames.fetch_add(frames, std::memory_order_relaxed);
        }
        return frames;
    }
    std::lock_guard<std::mutex> callback_lock(engine.callback_mutex);
    if (!core_callbacks_enabled() || !engine.backend)
        return frames;
    update_volume();
    if (new_retrorun_audio_buffer > 0)
        engine.target_frames = static_cast<size_t>(new_retrorun_audio_buffer);
    engine.staging.insert(engine.staging.end(), data, data + frames * CHANNELS);
    flush_staging_chunks();
    return frames;
}
