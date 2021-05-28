/*
retrorun-go2 - libretro frontend for the ODROID-GO Advance
Copyright (C) 2020  OtherCrashOverride

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

#include <stdio.h>
#include <string.h>

#include <go2/audio.h>

#define FRAMES_MAX (48000)
#define CHANNELS (2)

extern int opt_volume;


static go2_audio_t* audio;
static u_int16_t audioBuffer[FRAMES_MAX * CHANNELS];
static int audioFrameCount;
static int audioFrameLimit;
static int prevVolume;

void audio_init(int freq)
{
    // Note: audio stutters in OpenAL unless the buffer frequency at upload
    // is the same as during creation.
    audio = go2_audio_create(freq);
    audioFrameCount = 0;
    audioFrameLimit = 1.0 / 60.0 * freq;

    //printf("audio_init: freq=%d\n", freq);

    if (opt_volume > -1)
    {
        go2_audio_volume_set(audio, (uint32_t) opt_volume);
    }
    else
    {
        opt_volume = go2_audio_volume_get(audio);
    }
    prevVolume = opt_volume;
}

void audio_deinit()
{

}

static void SetVolume()
{
    if (opt_volume != prevVolume)
    {
        go2_audio_volume_set(audio, (uint32_t) opt_volume);
        prevVolume = opt_volume;
    }
}

void core_audio_sample(int16_t left, int16_t right)
{
    /*if (input_ffwd_requested)
        return;*/

    SetVolume();

	u_int32_t* ptr = (u_int32_t*)audioBuffer;
    ptr[audioFrameCount++] = (left << 16) | right;

    if (audioFrameCount >= audioFrameLimit)
    {
        go2_audio_submit(audio, (const short*)audioBuffer, audioFrameCount);
        audioFrameCount = 0;
    }
}

size_t core_audio_sample_batch(const int16_t * data, size_t frames)
{
    SetVolume();

    if (audioFrameCount + frames > audioFrameLimit)
    {
        go2_audio_submit(audio, (const short*)audioBuffer, audioFrameCount);
        audioFrameCount = 0;
    }

    memcpy(audioBuffer + (audioFrameCount * CHANNELS), data, frames * sizeof(int16_t) * CHANNELS);
    audioFrameCount += frames;

	return frames;
}
