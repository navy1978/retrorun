/*
retrorun - libretro frontend for Anbernic Devices
Copyright (C) 2020  OtherCrashOverride
Copyright (C) 2021-present  navy1978

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "audio.h"
#include "input.h"
#include "globals.h"
#include <unistd.h>
#include <stdio.h>
#include <cstring>

#include "platform.h"
#include <algorithm>
#include <mutex> // std::mutex
#include <condition_variable>
#include <deque>
#include <chrono>
#include <thread>
#include <vector>
#include <cmath>
#include <atomic>

#define FRAMES_MAX (48000)
#define CHANNELS (2)

// Calculate the size of the audioBuffer array
#define AUDIO_BUFFER_SIZE (FRAMES_MAX * CHANNELS)

extern int opt_volume;

static rr_audio_t *audio;
static u_int16_t audioBuffer[AUDIO_BUFFER_SIZE];

static int audioFrameCount;
static int audioFrameLimit;
static int prevVolume;
static std::atomic<uint64_t> fastForwardAudioDroppedFrames{0};

struct QueuedAudio
{
    std::vector<short> samples;
    int frames;
};

static std::mutex audioQueueMutex;
static std::condition_variable audioQueueReady;
static std::condition_variable audioQueueSpace;
static std::deque<QueuedAudio> audioQueue;
static std::thread audioThread;
static size_t queuedAudioFrames = 0;
static size_t maxQueuedAudioFrames = 0;
static bool stopAudioThread = false;

bool firstTime = true;
int init_freq;

std::string soundCardName;

static void audioThreadLoop()
{
    for (;;)
    {
        QueuedAudio chunk;
        {
            std::unique_lock<std::mutex> lock(audioQueueMutex);
            audioQueueReady.wait(lock, [] {
                return stopAudioThread || !audioQueue.empty();
            });

            if (stopAudioThread && audioQueue.empty())
            {
                rr_audio_release_thread(audio);
                return;
            }

            chunk = std::move(audioQueue.front());
            audioQueue.pop_front();
            queuedAudioFrames -= static_cast<size_t>(chunk.frames);
        }
        audioQueueSpace.notify_all();
        rr_audio_submit(audio, chunk.samples.data(), chunk.frames);
    }
}

static void submitAudio(const short *data, int frames)
{
    if (!audio || !data || frames <= 0)
        return;

    if (!forceAudioMultithread)
    {
        rr_audio_submit(audio, data, frames);
        return;
    }

    QueuedAudio chunk;
    chunk.frames = frames;
    chunk.samples.assign(data, data + static_cast<size_t>(frames) * CHANNELS);

    std::unique_lock<std::mutex> lock(audioQueueMutex);
    audioQueueSpace.wait(lock, [frames] {
        return stopAudioThread || audioQueue.empty() ||
               queuedAudioFrames + static_cast<size_t>(frames) <= maxQueuedAudioFrames;
    });
    if (stopAudioThread)
        return;

    queuedAudioFrames += static_cast<size_t>(frames);
    audioQueue.push_back(std::move(chunk));
    lock.unlock();
    audioQueueReady.notify_one();
}

void audio_init(int freq)
{
    // Note: audio stutters in OpenAL unless the buffer frequency at upload
    // is the same as during creation.
    init_freq = freq;
    audio = rr_audio_create(freq);
    audioFrameCount = 0;
    firstTime = true;

    if (forceAudioMultithread && audio)
    {
        // Four nominal 60 Hz chunks keep the core decoupled from backend
        // waits without allowing latency to grow without limit.
        maxQueuedAudioFrames = std::max<size_t>(2048, static_cast<size_t>(freq) / 15);
        {
            std::lock_guard<std::mutex> lock(audioQueueMutex);
            audioQueue.clear();
            queuedAudioFrames = 0;
            stopAudioThread = false;
        }
        audioThread = std::thread(audioThreadLoop);
        logger.log(Logger::INF,
                   "Threaded audio enabled: queue=%zu frames (%.1f ms)",
                   maxQueuedAudioFrames,
                   freq > 0 ? maxQueuedAudioFrames * 1000.0 / freq : 0.0);
    }
    bool is503AudioDeviceLike = isRG503() || isRG353M() || isRG353V();
    soundCardName = isRG552() ? "DAC" : is503AudioDeviceLike ? "Master"
                                                  : "Playback";

    if (opt_volume > -1)
    {
        rr_audio_volume_set(audio, (uint32_t)opt_volume, soundCardName.c_str());
    }
    else
    {
        opt_volume = rr_audio_volume_get(audio, soundCardName.c_str());
    }
    prevVolume = opt_volume;
}

void audio_deinit()
{
    if (audioThread.joinable())
    {
        {
            std::lock_guard<std::mutex> lock(audioQueueMutex);
            stopAudioThread = true;
            audioQueue.clear();
            queuedAudioFrames = 0;
        }
        audioQueueReady.notify_all();
        audioQueueSpace.notify_all();
        audioThread.join();
    }

    if (audio != NULL)
    {
        rr_audio_destroy(audio);
        audio = NULL;
    }
}

void audio_discard_pending()
{
    audioFrameCount = 0;
    if (!forceAudioMultithread)
        return;

    {
        std::lock_guard<std::mutex> lock(audioQueueMutex);
        audioQueue.clear();
        queuedAudioFrames = 0;
    }
    audioQueueSpace.notify_all();
}

static void SetVolume()
{
    if (opt_volume != prevVolume)
    {
        rr_audio_volume_set(audio, (uint32_t)opt_volume, soundCardName.c_str());
        prevVolume = opt_volume;
    }
}

int getVolume()
{
    int value = rr_audio_volume_get(audio, soundCardName.c_str());
    return value;
}

uint64_t fastForwardAudioFramesDropped()
{
    return fastForwardAudioDroppedFrames.exchange(0);
}

void setVolume(int value)
{
    rr_audio_volume_set(audio, (uint32_t)value, soundCardName.c_str());
}

void core_audio_sample(int16_t left, int16_t right)
{
    
    if (input_ffwd_requested || audio_disabled)
    {
        if (input_ffwd_requested)
            ++fastForwardAudioDroppedFrames;
        return;
    }

    SetVolume();

    u_int32_t *ptr = (u_int32_t *)audioBuffer;
    ptr[audioFrameCount++] = (left << 16) | right;

    if (audioFrameCount >= retrorun_audio_buffer)
    {
        submitAudio((const short *)audioBuffer, audioFrameCount);
        audioFrameCount = 0;
        retrorun_audio_buffer = new_retrorun_audio_buffer==-1 ? audioFrameLimit:new_retrorun_audio_buffer;
    }
}

#include <stdint.h>
#include <string.h>

size_t core_audio_sample_batch(const int16_t *data, size_t frames)
{
    if (input_ffwd_requested || audio_disabled)
        {
            if (input_ffwd_requested)
                fastForwardAudioDroppedFrames += frames;
            return frames;
        }

    if (firstTime && originalFps > 0)
    {
        logger.log(Logger::DEB, "(Audio init) config...");
        audioFrameLimit = 1.0 / originalFps * init_freq;

        if (retrorun_audio_buffer == -1)
        {
            retrorun_audio_buffer = audioFrameLimit;
        }
        logger.log(Logger::DEB, "(Audio init)- originalFps:%f", originalFps);
        logger.log(Logger::DEB, "(Audio init)- audioFrameLimit:%d", audioFrameLimit);
        logger.log(Logger::DEB, "(Audio init)- retrorun_audio_buffer:%d", retrorun_audio_buffer);
        firstTime = false;
    }

    if (originalFps < 1)
    {
        logger.log(Logger::DEB, "ORIGINAL FPS NOT VALID! skipping audio");
        return frames;
    }
    SetVolume();

    int currentFrame = (int)frames;

    if (currentFrame > FRAMES_MAX || currentFrame < 1)
    {
        logger.log(Logger::DEB, "AUDIO FRAME NOT VALID! skipping audio");
        return frames;
    }

    if (audioFrameCount + frames > static_cast<size_t>(retrorun_audio_buffer))
    // if (audioFrameCount + frames > retrorun_audio_buffer)
    {
        submitAudio((const short *)audioBuffer, audioFrameCount);
        audioFrameCount = 0;
        retrorun_audio_buffer = new_retrorun_audio_buffer==-1 ? audioFrameLimit :new_retrorun_audio_buffer;
    }
 
    size_t size = frames * sizeof(int16_t) * CHANNELS;
    // libc selects the best implementation for the active ARM/x86 CPU and
    // safely handles alignment that the former uint64_t loop assumed.
    std::memcpy(audioBuffer + (audioFrameCount * CHANNELS), data, size);
    audioFrameCount += frames;
    return frames;
}
