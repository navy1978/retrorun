#include "audio.h"
#include "benchmark.h"
#include "logger.h"
#include "platform.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

Logger logger(Logger::ERR);
int opt_volume = -1;
int retrorun_audio_buffer = 4;
int new_retrorun_audio_buffer = -1;
bool retrorun_audio_stable_buffer = false;
bool forceAudioMultithread = true;
bool input_ffwd_requested = false;
bool audio_disabled = false;

bool isRG552() { return false; }
bool isRK3566Device() { return false; }
bool core_callbacks_enabled() { return true; }

struct rr_audio {
    int frequency = 0;
    std::atomic<bool> cancelled{false};
    std::atomic<bool> paused{false};
};

namespace fake {
std::mutex mutex;
std::condition_variable changed;
bool block_submit = false;
bool submit_entered = false;
uint64_t submitted_frames = 0;
uint64_t flushes = 0;
uint64_t pauses = 0;
uint64_t resumes = 0;
uint64_t creates = 0;
int last_frequency = 0;

void reset()
{
    std::lock_guard<std::mutex> lock(mutex);
    block_submit = false;
    submit_entered = false;
    submitted_frames = 0;
    flushes = 0;
    pauses = 0;
    resumes = 0;
    creates = 0;
    last_frequency = 0;
}

void wait_for_submit()
{
    std::unique_lock<std::mutex> lock(mutex);
    assert(changed.wait_for(lock, std::chrono::seconds(2), [] { return submit_entered; }));
}

void release_submit()
{
    std::lock_guard<std::mutex> lock(mutex);
    block_submit = false;
    changed.notify_all();
}
}

rr_audio_t* rr_audio_create(int frequency)
{
    auto* audio = new rr_audio;
    audio->frequency = frequency;
    std::lock_guard<std::mutex> lock(fake::mutex);
    ++fake::creates;
    fake::last_frequency = frequency;
    return audio;
}

void rr_audio_destroy(rr_audio_t* audio) { delete audio; }
bool rr_audio_valid(rr_audio_t* audio) { return audio != nullptr; }
void rr_audio_release_thread(rr_audio_t*) {}

bool rr_audio_submit(rr_audio_t* audio, const short*, int frames)
{
    std::unique_lock<std::mutex> lock(fake::mutex);
    fake::submit_entered = true;
    fake::changed.notify_all();
    fake::changed.wait(lock, [&] {
        return !fake::block_submit || audio->cancelled.load(std::memory_order_relaxed);
    });
    if (audio->cancelled.load(std::memory_order_relaxed))
        return false;
    fake::submitted_frames += static_cast<uint64_t>(frames);
    return true;
}

void rr_audio_flush(rr_audio_t*)
{
    std::lock_guard<std::mutex> lock(fake::mutex);
    ++fake::flushes;
}

void rr_audio_pause(rr_audio_t* audio, bool paused)
{
    audio->paused.store(paused, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(fake::mutex);
    if (paused) ++fake::pauses;
    else ++fake::resumes;
}

void rr_audio_cancel(rr_audio_t* audio)
{
    audio->cancelled.store(true, std::memory_order_relaxed);
    fake::changed.notify_all();
}

void rr_audio_diagnostics_get(rr_audio_t*, rr_audio_diagnostics_t* diagnostics)
{
    if (diagnostics) *diagnostics = {};
}

uint32_t rr_audio_volume_get(rr_audio_t*, const char*) { return 50; }
void rr_audio_volume_set(rr_audio_t*, uint32_t, const char*) {}

static std::vector<int16_t> samples(size_t frames)
{
    return std::vector<int16_t>(frames * 2, 123);
}

static void test_single_sample_core()
{
    fake::reset();
    forceAudioMultithread = false;
    retrorun_audio_buffer = 4;
    audio_init(1000, 250.0);
    for (int i = 0; i < 4; ++i)
        core_audio_sample(10, -10);
    assert(fake::submitted_frames == 4);
    audio_deinit();
}

static void test_backpressure_without_loss()
{
    fake::reset();
    forceAudioMultithread = true;
    retrorun_audio_buffer = 256;
    {
        std::lock_guard<std::mutex> lock(fake::mutex);
        fake::block_submit = true;
    }
    audio_init(48000, 60.0);
    auto data = samples(4096);
    auto producer = std::async(std::launch::async, [&] {
        return core_audio_sample_batch(data.data(), 4096);
    });
    fake::wait_for_submit();
    assert(producer.wait_for(std::chrono::milliseconds(30)) == std::future_status::timeout);
    fake::release_submit();
    assert(producer.get() == 4096);
    assert(audio_flush());
    assert(fake::submitted_frames == 4096);
    audio_deinit();
}

static void test_flush_pause_resume()
{
    fake::reset();
    forceAudioMultithread = true;
    retrorun_audio_buffer = 256;
    {
        std::lock_guard<std::mutex> lock(fake::mutex);
        fake::block_submit = true;
    }
    audio_init(48000, 60.0);
    auto data = samples(512);
    core_audio_sample_batch(data.data(), 512);
    fake::wait_for_submit();
    auto flushing = std::async(std::launch::async, [] { return audio_flush(); });
    assert(flushing.wait_for(std::chrono::milliseconds(30)) == std::future_status::timeout);
    fake::release_submit();
    assert(flushing.get());
    for (int i = 0; i < 5; ++i) {
        assert(audio_pause());
        assert(audio_resume());
    }
    assert(fake::flushes >= 6);
    assert(fake::pauses == 5);
    assert(fake::resumes == 5);
    audio_deinit();
}

static void test_stop_while_backend_waits()
{
    fake::reset();
    forceAudioMultithread = true;
    retrorun_audio_buffer = 256;
    {
        std::lock_guard<std::mutex> lock(fake::mutex);
        fake::block_submit = true;
    }
    audio_init(48000, 60.0);
    auto data = samples(512);
    core_audio_sample_batch(data.data(), 512);
    fake::wait_for_submit();
    auto stopping = std::async(std::launch::async, [] { audio_deinit(); });
    assert(stopping.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    stopping.get();
}

static void test_runtime_reconfigure_and_repeated_shutdown()
{
    fake::reset();
    forceAudioMultithread = true;
    retrorun_audio_buffer = 4;
    for (int i = 0; i < 3; ++i) {
        audio_init(44100, 60.0);
        assert(audio_reconfigure(44100, 50.0));
        audio_deinit();
    }
    audio_init(44100, 60.0);
    assert(audio_reconfigure(48000, 59.94));
    assert(fake::last_frequency == 48000);
    audio_deinit();
    assert(fake::creates == 5);
}

int main()
{
    test_single_sample_core();
    test_backpressure_without_loss();
    test_flush_pause_resume();
    test_stop_while_backend_waits();
    test_runtime_reconfigure_and_repeated_shutdown();
    return 0;
}
