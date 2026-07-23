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
#include <atomic>
#include <algorithm>
#include <deque>
#include <limits>
#include <new>
#include <vector>

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
    int buffer_frames[STABLE_BUFFER_COUNT];
    std::deque<ALuint> queue_order;
    int buffer_count;
    int stretch_percent;
    int stretch_low_ms;
    uint64_t stretch_low_frames;
    bool submitted_once;
    std::vector<short> stretch_buffer;
    bool isAudioInitialized;
    std::atomic<bool> cancelled{false};
    std::atomic<bool> paused{false};
    std::atomic<uint64_t> underruns{0};
    std::atomic<uint64_t> overruns{0};
    std::atomic<uint64_t> frames_dropped{0};
    std::atomic<uint64_t> max_queue_depth{0};
    std::atomic<uint64_t> min_queue_depth{std::numeric_limits<uint64_t>::max()};
    std::atomic<uint64_t> queue_depth_samples{0};
    std::atomic<uint64_t> queue_depth_total_frames{0};
    std::atomic<uint64_t> queue_empty_observations{0};
    std::atomic<uint64_t> queue_low_observations{0};
    std::atomic<uint64_t> adaptive_stretch_frames{0};
} go2_audio_t;

static int environment_integer(const char *name, int fallback, int maximum)
{
    const char *value = getenv(name);
    if (!value || !*value)
        return fallback;
    char *end = NULL;
    const long parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0')
        return fallback;
    return static_cast<int>(std::max<long>(
        0, std::min<long>(parsed, maximum)));
}

static int buffer_index(const go2_audio_t *audio, ALuint buffer)
{
    for (int i = 0; i < audio->buffer_count; ++i)
        if (audio->buffers[i] == buffer)
            return i;
    return -1;
}

static void remove_queued_buffer(go2_audio_t *audio, ALuint buffer)
{
    const auto entry = std::find(audio->queue_order.begin(),
                                 audio->queue_order.end(), buffer);
    if (entry != audio->queue_order.end())
        audio->queue_order.erase(entry);
}

// OpenAL reports how many buffers at the front of the queue have already
// played. Their data must not count toward the amount of audio still pending.
static uint64_t queued_audio_frames(const go2_audio_t *audio, ALint processed)
{
    uint64_t frames = 0;
    size_t position = 0;
    const size_t consumed = static_cast<size_t>(std::max(0, processed));
    for (ALuint buffer : audio->queue_order)
    {
        if (position++ < consumed)
            continue;
        const int index = buffer_index(audio, buffer);
        if (index >= 0)
            frames += static_cast<uint64_t>(
                std::max(0, audio->buffer_frames[index]));
    }
    return frames;
}

static void update_max_queue_depth(go2_audio_t *audio, uint64_t queued_frames)
{
    uint64_t current_max =
        audio->max_queue_depth.load(std::memory_order_relaxed);
    while (current_max < queued_frames &&
           !audio->max_queue_depth.compare_exchange_weak(
               current_max, queued_frames, std::memory_order_relaxed)) {}
}

static void record_queue_depth(go2_audio_t *audio, uint64_t queued_frames)
{
    update_max_queue_depth(audio, queued_frames);
    audio->queue_depth_samples.fetch_add(1, std::memory_order_relaxed);
    audio->queue_depth_total_frames.fetch_add(queued_frames,
                                               std::memory_order_relaxed);
    if (!audio->submitted_once)
        return;

    uint64_t current_min =
        audio->min_queue_depth.load(std::memory_order_relaxed);
    while (current_min > queued_frames &&
           !audio->min_queue_depth.compare_exchange_weak(
               current_min, queued_frames, std::memory_order_relaxed)) {}

    if (!audio->paused.load(std::memory_order_relaxed))
    {
        if (queued_frames == 0)
            audio->queue_empty_observations.fetch_add(
                1, std::memory_order_relaxed);
        if (queued_frames < audio->stretch_low_frames)
            audio->queue_low_observations.fetch_add(
                1, std::memory_order_relaxed);
    }
}

go2_audio_t *go2_audio_create(int frequency)
{

    go2_audio_t *result = new (std::nothrow) go2_audio_t{};
    if (!result)
    {
        logger.log(Logger::ERR,"malloc failed.\n");
        return NULL;
    }

    result->frequency = frequency;
    result->buffer_count = retrorun_audio_stable_buffer
        ? STABLE_BUFFER_COUNT : DEFAULT_BUFFER_COUNT;
    result->stretch_percent = retrorun_audio_stable_buffer ? 0 :
        environment_integer("RETRORUN_GO2_AUDIO_STRETCH_PERCENT",
                            retrorun_go2_audio_stretch_percent, 10);
    result->stretch_low_ms = environment_integer(
        "RETRORUN_GO2_AUDIO_STRETCH_LOW_MS",
        retrorun_go2_audio_stretch_low_ms, 200);
    result->stretch_low_frames =
        static_cast<uint64_t>(frequency) *
        static_cast<uint64_t>(result->stretch_low_ms) / 1000;
    // Record resolved environment overrides in benchmark metadata.
    retrorun_go2_audio_stretch_percent = result->stretch_percent;
    retrorun_go2_audio_stretch_low_ms = result->stretch_low_ms;

    result->device = alcOpenDevice(NULL);
    if (!result->device)
    {
        logger.log(Logger::ERR,"alcOpenDevice failed.\n");
        delete result;
        return NULL;
    }

    result->context = alcCreateContext(result->device, NULL);
    if (!alcMakeContextCurrent(result->context))
    {
        alcCloseDevice(result->device);
        delete result;
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
        result->buffer_frames[i] = 0;
        result->queue_order.push_back(result->buffers[i]);
    }

    alSourcePlay(result->source);

    result->isAudioInitialized = true;

    // Audio submission may run on RetroRun's optional worker. Do not leave
    // the OpenAL context associated with the thread that created it.
    alcMakeContextCurrent(NULL);

    logger.log(Logger::INF,
               "GO2 audio: backend=openal, stable_buffer=%s, frequency=%d, "
               "buffers=%d, stretch_percent=%d, stretch_low_ms=%d.",
               retrorun_audio_stable_buffer ? "on" : "off",
               result->frequency, result->buffer_count,
               result->stretch_percent, result->stretch_low_ms);

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

    delete audio;
}

std::mutex myMutex;

inline bool playAudio(go2_audio_t *audio, const short *data, int frames)
{

    if (!audio || !audio->isAudioInitialized || !data || frames <= 0)
        return false;

    if (alcGetCurrentContext() != audio->context &&
        !alcMakeContextCurrent(audio->context))
    {
        //printf("alcMakeContextCurrent failed.\n");
        return false;
    }

    

    ALint processedA = 0;
    const auto wait_started = std::chrono::steady_clock::now();
    while (!processedA && !audio->cancelled.load(std::memory_order_relaxed))
    {
        alGetSourceiv(audio->source, AL_BUFFERS_PROCESSED, &processedA);
        if (!processedA)
            // OpenAL has no processed-buffer event API. A short sleep keeps
            // this producer paced without burning an entire RK3326 core.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (std::chrono::steady_clock::now() - wait_started > std::chrono::seconds(2))
        {
            audio->overruns.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
    }
    if (audio->cancelled.load(std::memory_order_relaxed))
        return false;

    // Refresh the count after waiting: more than one buffer may have completed.
    alGetSourceiv(audio->source, AL_BUFFERS_PROCESSED, &processedA);
    const uint64_t queued_frames = queued_audio_frames(audio, processedA);
    record_queue_depth(audio, queued_frames);

    ALuint openALBufferID;
    alSourceUnqueueBuffers(audio->source, 1, &openALBufferID);
    remove_queued_buffer(audio, openALBufferID);

    ALuint format = AL_FORMAT_STEREO16;

    const short *queue_data = data;
    int queue_frames = frames;
    if (audio->stretch_percent > 0 && audio->submitted_once &&
        queued_frames < audio->stretch_low_frames &&
        !audio->paused.load(std::memory_order_relaxed))
    {
        queue_frames = frames + std::max(
            1, frames * audio->stretch_percent / 100);
        audio->stretch_buffer.resize(
            static_cast<size_t>(queue_frames) * SOUND_CHANNEL_COUNT);
        const uint64_t phase_step = queue_frames > 1
            ? (static_cast<uint64_t>(frames - 1) << 16) /
                  static_cast<uint64_t>(queue_frames - 1)
            : 0;
        uint64_t phase = 0;
        for (int output_frame = 0;
             output_frame < queue_frames; ++output_frame)
        {
            const int source_frame =
                std::min<int>(phase >> 16, frames - 1);
            const int next_frame =
                std::min(source_frame + 1, frames - 1);
            const int fraction = static_cast<int>(phase & 0xffff);
            for (int channel = 0; channel < SOUND_CHANNEL_COUNT; ++channel)
            {
                const int first =
                    data[source_frame * SOUND_CHANNEL_COUNT + channel];
                const int second =
                    data[next_frame * SOUND_CHANNEL_COUNT + channel];
                audio->stretch_buffer[
                    output_frame * SOUND_CHANNEL_COUNT + channel] =
                    static_cast<short>(
                        first +
                        ((static_cast<int64_t>(second - first) *
                          fraction) >> 16));
            }
            phase += phase_step;
        }
        queue_data = audio->stretch_buffer.data();
        audio->adaptive_stretch_frames.fetch_add(
            static_cast<uint64_t>(queue_frames - frames),
            std::memory_order_relaxed);
    }

    const int dataByteLength =
        queue_frames * sizeof(short) * SOUND_CHANNEL_COUNT;
    alBufferData(openALBufferID, format, queue_data,
                 dataByteLength, audio->frequency);

    const int index = buffer_index(audio, openALBufferID);
    if (index >= 0)
        audio->buffer_frames[index] = queue_frames;

    alSourceQueueBuffers(audio->source, 1, &openALBufferID);
    audio->queue_order.push_back(openALBufferID);
    audio->submitted_once = true;

    ALint processed_after = 0;
    alGetSourceiv(audio->source, AL_BUFFERS_PROCESSED, &processed_after);
    update_max_queue_depth(
        audio, queued_audio_frames(audio, processed_after));

    ALint result;
    alGetSourcei(audio->source, AL_SOURCE_STATE, &result);

    if (result != AL_PLAYING)
    {
        if (!audio->paused.load(std::memory_order_relaxed))
        {
            audio->underruns.fetch_add(1, std::memory_order_relaxed);
            alSourcePlay(audio->source);
        }
    }
    return alGetError() == AL_NO_ERROR;
}

bool go2_audio_submit(go2_audio_t *audio, const short *data, int frames)
{
    std::lock_guard<std::mutex> lock(myMutex);
    return playAudio(audio, data, frames);
}

void go2_audio_release_thread(go2_audio_t *audio)
{
    if (audio && alcGetCurrentContext() == audio->context)
        alcMakeContextCurrent(NULL);
}

bool go2_audio_valid(go2_audio_t *audio)
{
    return audio && audio->isAudioInitialized && audio->device && audio->context;
}

void go2_audio_flush(go2_audio_t *audio)
{
    if (!go2_audio_valid(audio)) return;
    std::lock_guard<std::mutex> lock(myMutex);
    if (alcGetCurrentContext() != audio->context && !alcMakeContextCurrent(audio->context))
        return;
    alSourceStop(audio->source);
    ALint queued = 0;
    alGetSourcei(audio->source, AL_BUFFERS_QUEUED, &queued);
    while (queued-- > 0) {
        ALuint buffer = 0;
        alSourceUnqueueBuffers(audio->source, 1, &buffer);
    }
    audio->queue_order.clear();
    for (int i = 0; i < audio->buffer_count; ++i) {
        alBufferData(audio->buffers[i], AL_FORMAT_STEREO16, NULL, 0, audio->frequency);
        alSourceQueueBuffers(audio->source, 1, &audio->buffers[i]);
        audio->buffer_frames[i] = 0;
        audio->queue_order.push_back(audio->buffers[i]);
    }
    audio->submitted_once = false;
    if (!audio->paused.load(std::memory_order_relaxed) &&
        !audio->cancelled.load(std::memory_order_relaxed))
        alSourcePlay(audio->source);
}

void go2_audio_pause(go2_audio_t *audio, bool paused)
{
    if (!go2_audio_valid(audio)) return;
    std::lock_guard<std::mutex> lock(myMutex);
    audio->paused.store(paused, std::memory_order_relaxed);
    if (alcGetCurrentContext() != audio->context && !alcMakeContextCurrent(audio->context))
        return;
    if (paused) alSourcePause(audio->source);
    else if (!audio->cancelled.load(std::memory_order_relaxed)) alSourcePlay(audio->source);
}

void go2_audio_cancel(go2_audio_t *audio)
{
    if (audio) audio->cancelled.store(true, std::memory_order_relaxed);
}

void go2_audio_diagnostics_get(go2_audio_t *audio, uint64_t *underruns,
                               uint64_t *overruns, uint64_t *frames_dropped,
                               uint64_t *max_queue_depth,
                               uint64_t *min_queue_depth,
                               uint64_t *queue_depth_samples,
                               uint64_t *queue_depth_total_frames,
                               uint64_t *queue_empty_observations,
                               uint64_t *queue_low_observations,
                               uint64_t *adaptive_stretch_frames)
{
    if (underruns) *underruns = audio ? audio->underruns.load() : 0;
    if (overruns) *overruns = audio ? audio->overruns.load() : 0;
    if (frames_dropped) *frames_dropped = audio ? audio->frames_dropped.load() : 0;
    if (max_queue_depth) *max_queue_depth = audio ? audio->max_queue_depth.load() : 0;
    if (min_queue_depth) {
        const uint64_t value = audio ? audio->min_queue_depth.load() : 0;
        *min_queue_depth =
            value == std::numeric_limits<uint64_t>::max() ? 0 : value;
    }
    if (queue_depth_samples)
        *queue_depth_samples = audio ? audio->queue_depth_samples.load() : 0;
    if (queue_depth_total_frames)
        *queue_depth_total_frames =
            audio ? audio->queue_depth_total_frames.load() : 0;
    if (queue_empty_observations)
        *queue_empty_observations =
            audio ? audio->queue_empty_observations.load() : 0;
    if (queue_low_observations)
        *queue_low_observations =
            audio ? audio->queue_low_observations.load() : 0;
    if (adaptive_stretch_frames)
        *adaptive_stretch_frames =
            audio ? audio->adaptive_stretch_frames.load() : 0;
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
