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
#include <mutex> // std::mutex
#include <chrono>
#include <thread>
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
std::mutex mtx; // mutex for critical section

bool firstTime = true;
int init_freq;

std::string soundCardName;

void audio_init(int freq)
{
    // Note: audio stutters in OpenAL unless the buffer frequency at upload
    // is the same as during creation.
    init_freq = freq;
    audio = rr_audio_create(freq);
    audioFrameCount = 0;
    // The Pocket 1 uses the RK3566 mixer layout, like the RG353 family:
    // its playback volume control is named "Master", not "Playback".
    bool is503AudioDeviceLike = isRK3566Device();
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
    if (audio != NULL)
        rr_audio_destroy(audio);
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
        rr_audio_submit(audio, (const short *)audioBuffer, audioFrameCount);
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
        rr_audio_submit(audio, (const short *)audioBuffer, audioFrameCount);
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
