/*
libgo2 - Support library for the ODROID-GO Advance
Copyright (C) 2020 OtherCrashOverride
Copyright (C) 2023-present  navy1978

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "audio.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <alsa/asoundlib.h>
#include <alsa/mixer.h>
#include "../globals.h"

#include <thread>
#include <chrono>
#include <mutex>

#define SOUND_SAMPLES_SIZE (2048)
#define SOUND_CHANNEL_COUNT 2

static constexpr int DEFAULT_BUFFER_COUNT = 4;
static constexpr int STABLE_BUFFER_COUNT = 6;

typedef struct go2_audio
{
    ALsizei frequency;
    ALCdevice *device;
    ALCcontext *context;
    ALuint source;
    ALuint buffers[STABLE_BUFFER_COUNT];
    int buffer_count;
    bool isAudioInitialized;
} go2_audio_t;

go2_audio_t *go2_audio_create(int frequency)
{

    go2_audio_t *result = (go2_audio_t *)malloc(sizeof(*result));
    if (!result)
    {
        logger.log(Logger::ERR,"malloc failed.\n");
        free(result);
        return NULL;
    }

    memset(result, 0, sizeof(*result));

    result->frequency = frequency;
    result->buffer_count = retrorun_audio_stable_buffer
        ? STABLE_BUFFER_COUNT : DEFAULT_BUFFER_COUNT;

    result->device = alcOpenDevice(NULL);
    if (!result->device)
    {
        logger.log(Logger::ERR,"alcOpenDevice failed.\n");
        free(result);
        return NULL;
    }

    result->context = alcCreateContext(result->device, NULL);
    if (!alcMakeContextCurrent(result->context))
    {
        alcCloseDevice(result->device);
        free(result);
        return NULL;
    }

    alGenSources((ALuint)1, &result->source);

    alSourcef(result->source, AL_PITCH, 1);
    alSourcef(result->source, AL_GAIN, 1);
    alSource3f(result->source, AL_POSITION, 0, 0, 0);
    alSource3f(result->source, AL_VELOCITY, 0, 0, 0);
    alSourcei(result->source, AL_LOOPING, AL_FALSE);


    for (int i = 0; i < result->buffer_count; ++i)
    {
        alGenBuffers(1, &result->buffers[i]);
        alBufferData(result->buffers[i], AL_FORMAT_STEREO16, NULL, 0, frequency);
        alSourceQueueBuffers(result->source, 1, &result->buffers[i]);
    }

    alSourcePlay(result->source);

    result->isAudioInitialized = true;

    

    return result;
}

void go2_audio_destroy(go2_audio_t *audio)
{
    if (!audio)
        return;
    if (alcGetCurrentContext() != audio->context)
        alcMakeContextCurrent(audio->context);
    alSourceStop(audio->source);
    alDeleteSources(1, &audio->source);
    alDeleteBuffers(audio->buffer_count, audio->buffers);
    alcMakeContextCurrent(NULL);
    alcDestroyContext(audio->context);
    alcCloseDevice(audio->device);

    free(audio);
}

std::mutex myMutex;

inline void playAudio(go2_audio_t *audio, const short *data, int frames)
{

    if (!audio || !audio->isAudioInitialized)
        return;

    if (alcGetCurrentContext() != audio->context &&
        !alcMakeContextCurrent(audio->context))
    {
        //printf("alcMakeContextCurrent failed.\n");
        return;
    }

    

    ALint processedA = 0;
    while (!processedA)
    {
        alGetSourceiv(audio->source, AL_BUFFERS_PROCESSED, &processedA);
        if (!processedA)
            // OpenAL has no processed-buffer event API. A short sleep keeps
            // this producer paced without burning an entire RK3326 core.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ALuint openALBufferID;
    alSourceUnqueueBuffers(audio->source, 1, &openALBufferID);

    ALuint format = AL_FORMAT_STEREO16;

    int dataByteLength = frames * sizeof(short) * SOUND_CHANNEL_COUNT;
    alBufferData(openALBufferID, format, data, dataByteLength, audio->frequency);

    alSourceQueueBuffers(audio->source, 1, &openALBufferID);

    ALint result;
    alGetSourcei(audio->source, AL_SOURCE_STATE, &result);

    if (result != AL_PLAYING)
    {

        alSourcePlay(audio->source);
    }
}
auto prevClock = std::chrono::high_resolution_clock::now();
auto totClock = std::chrono::high_resolution_clock::now();
auto max_fps = originalFps;

void go2_audio_submit(go2_audio_t *audio, const short *data, int frames)
{
    std::lock_guard<std::mutex> lock(myMutex);
    max_fps = originalFps < 20 ? 60 : originalFps;
    playAudio(audio, data, frames);

    auto nextClock = std::chrono::high_resolution_clock::now();
    // make sure each frame takes *at least* 1/60th of a second
    // auto frameClock = std::chrono::high_resolution_clock::now();
    double deltaTime = (nextClock - prevClock).count() / 1e9;
    double sleepSecs = 1.0 / max_fps - deltaTime;

    if (sleepSecs > 0)
    {
        // printf("-RR- waiting!\n");
        //  std::this_thread::sleep_for(std::chrono::nanoseconds((int64_t)(sleepSecs * 1e9)));
    }
    prevClock = nextClock;
    totClock = std::chrono::high_resolution_clock::now();

}

static snd_mixer_elem_t *go2_audio_open_mixer_element(snd_mixer_t **handle,
                                                        const char *selem_name)
{
    const char *card = "default";
    snd_mixer_selem_id_t *sid;

    *handle = NULL;
    if (snd_mixer_open(handle, 0) < 0 ||
        snd_mixer_attach(*handle, card) < 0 ||
        snd_mixer_selem_register(*handle, NULL, NULL) < 0 ||
        snd_mixer_load(*handle) < 0)
    {
        if (*handle)
            snd_mixer_close(*handle);
        *handle = NULL;
        logger.log(Logger::WARN, "Unable to open ALSA mixer control '%s'.", selem_name);
        return NULL;
    }

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, selem_name);

    snd_mixer_elem_t *elem = snd_mixer_find_selem(*handle, sid);
    if (!elem)
        logger.log(Logger::WARN, "ALSA mixer control '%s' not found; volume control disabled.", selem_name);
    return elem;
}

uint32_t go2_audio_volume_get(go2_audio_t *audio, const char *selem_name)
{
    snd_mixer_t *handle;
    snd_mixer_elem_t *elem = go2_audio_open_mixer_element(&handle, selem_name);
    if (!elem)
        return 100;

    long min;
    long max;
    if (snd_mixer_selem_get_playback_volume_range(elem, &min, &max) < 0 || max <= min)
    {
        snd_mixer_close(handle);
        return 100;
    }

    // snd_mixer_selem_set_playback_volume_all(elem, value / 100.0f * max);
    long volume;
    if (snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &volume) < 0)
    {
        snd_mixer_close(handle);
        return 100;
    }

    snd_mixer_close(handle);

    uint32_t result = volume / (float)max * 100.0f;
    // printf("volume: min=%ld, max=%ld, volume=%ld, result=%d\n", min, max, volume, result);

    return result;
}

void go2_audio_volume_set(go2_audio_t *audio, uint32_t value, const char *selem_name)
{
    // https://gist.github.com/wolfg1969/3575700

    snd_mixer_t *handle;
    snd_mixer_elem_t *elem = go2_audio_open_mixer_element(&handle, selem_name);
    if (!elem)
        return;

    long min;
    long max;
    if (snd_mixer_selem_get_playback_volume_range(elem, &min, &max) < 0 || max <= min)
    {
        snd_mixer_close(handle);
        return;
    }
    // printf("volume: min=%ld, max=%ld\n", min, max);

    snd_mixer_selem_set_playback_volume_all(elem, value / 100.0f * max);

    snd_mixer_close(handle);
}

go2_audio_path_t go2_audio_path_get(go2_audio_t *audio, const char *selem_name)
{
    snd_mixer_t *handle;
    snd_mixer_elem_t *elem = go2_audio_open_mixer_element(&handle, selem_name);
    if (!elem)
        return (go2_audio_path_t)0;

    unsigned int value;
    if (snd_mixer_selem_get_enum_item(elem, SND_MIXER_SCHN_MONO, &value) < 0)
    {
        snd_mixer_close(handle);
        return (go2_audio_path_t)0;
    }

    // char name[128];
    // snd_mixer_selem_get_enum_item_name(elem, value, 128, name);
    // printf("audio path: value=%d [%s]\n", value, name);

    snd_mixer_close(handle);

    return (go2_audio_path_t)value;
}

void go2_audio_path_set(go2_audio_t *audio, go2_audio_path_t value, const char *selem_name)
{
    snd_mixer_t *handle;
    snd_mixer_elem_t *elem = go2_audio_open_mixer_element(&handle, selem_name);
    if (!elem)
        return;

    snd_mixer_selem_set_enum_item(elem, SND_MIXER_SCHN_MONO, (unsigned int)value);

    snd_mixer_close(handle);
}
