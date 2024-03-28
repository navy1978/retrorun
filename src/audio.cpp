/*
retrorun-go2 - libretro frontend for the ODROID-GO Advance
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
#include <string.h>

#include <go2/audio.h>
#include <mutex> // std::mutex
#include <chrono>
#include <thread>
#include <cmath>

#define FRAMES_MAX (48000)
#define CHANNELS (2)

// Calculate the size of the audioBuffer array
#define AUDIO_BUFFER_SIZE (FRAMES_MAX * CHANNELS)

extern int opt_volume;

static go2_audio_t *audio;
static u_int16_t audioBuffer[AUDIO_BUFFER_SIZE];

static int audioFrameCount;
static int audioFrameLimit;
static int prevVolume;
std::mutex mtx; // mutex for critical section

bool firstTime = true;
int init_freq;

std::string soundCardName;

void audio_init(int freq)
{
    // Note: audio stutters in OpenAL unless the buffer frequency at upload
    // is the same as during creation.
    init_freq = freq;
    audio = go2_audio_create(freq);
    audioFrameCount = 0;

    soundCardName = isRG552() ? "DAC" : isRG503() ? "Master"
                                                  : "Playback";

    if (opt_volume > -1)
    {
        go2_audio_volume_set(audio, (uint32_t)opt_volume, soundCardName.c_str());
    }
    else
    {
        opt_volume = go2_audio_volume_get(audio, soundCardName.c_str());
    }
    prevVolume = opt_volume;
}

void audio_deinit()
{
    if (audio != NULL)
        go2_audio_destroy(audio);
}

static void SetVolume()
{
    if (opt_volume != prevVolume)
    {
        go2_audio_volume_set(audio, (uint32_t)opt_volume, soundCardName.c_str());
        prevVolume = opt_volume;
    }
}

int getVolume()
{
    int value = go2_audio_volume_get(audio, soundCardName.c_str());
    return value;
}

void setVolume(int value)
{
    go2_audio_volume_set(audio, (uint32_t)value, soundCardName.c_str());
}

void core_audio_sample(int16_t left, int16_t right)
{
    
    if (input_ffwd_requested || audio_disabled)
    {
        return;
    }

    SetVolume();

    u_int32_t *ptr = (u_int32_t *)audioBuffer;
    ptr[audioFrameCount++] = (left << 16) | right;

    if (audioFrameCount >= retrorun_audio_buffer)
    {
        go2_audio_submit(audio, (const short *)audioBuffer, audioFrameCount);
        audioFrameCount = 0;
        retrorun_audio_buffer = new_retrorun_audio_buffer==-1 ? audioFrameLimit:new_retrorun_audio_buffer;
    }
}

#include <stdint.h>
#include <string.h>

static inline void newmemcpy(void *__restrict__ dstp,
                             void *__restrict__ srcp,
                             uint len)
{
    uint64_t *dst = (uint64_t *)dstp;
    uint64_t *src = (uint64_t *)srcp;
    size_t i, tail;

    // Check if len is at least one 64-bit chunk in size
    if (len >= sizeof(uint64_t))
    {
        // Copy 64-bit chunks
        for (i = 0; i < (len / sizeof(uint64_t)); i++)
            *dst++ = *src++;
    }

    // Copy remaining bytes (if any)
    tail = len & (sizeof(uint64_t) - 1);
    if (tail)
    {
        unsigned char *dstb = (unsigned char *)dst;
        unsigned char *srcb = (unsigned char *)src;
        memmove(dstb, srcb, tail);
    }
}

size_t core_audio_sample_batch(const int16_t *data, size_t frames)
{

    if (audio_disabled)
        {
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
    audioCounter++;
    // the following is for Fast Forwarding
    if (audioCounter != audioCounterSkip)
    {
        if (input_ffwd_requested)
        {
            return frames;
        }
    }
    else
    {
        audioCounter = 0;
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
        go2_audio_submit(audio, (const short *)audioBuffer, audioFrameCount);
        audioFrameCount = 0;
        retrorun_audio_buffer = new_retrorun_audio_buffer==-1 ? audioFrameLimit :new_retrorun_audio_buffer;
    }

    size_t size = frames * sizeof(int16_t) * CHANNELS;
    newmemcpy(audioBuffer + (audioFrameCount * CHANNELS), (void *)data, size);
    audioFrameCount += frames;
    return frames;
}
