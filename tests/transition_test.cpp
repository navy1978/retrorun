#include "core_loader.h"
#include "disk_control.h"
#include "logger.h"
#include "savestate.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <string>
#include <thread>

Logger logger(Logger::ERR);
RetroCore g_retro = {};
pthread_t main_thread_id;
bool input_slot_memory_save_done = false;
bool input_slot_memory_load_done = false;
bool input_slot_memory_load_requested = false;
bool input_pause_requested = false;
bool pause_requested = false;
bool lastLoadSaveStateDoneOk = true;
double lastLoadSaveStateDoneTime = 0.0;

namespace {
int audio_flushes = 0;
int video_barriers = 0;
int unserializes = 0;
int serialize_size_calls = 0;
int disk_step = 0;
bool ejected = false;
bool flycast2021 = false;
bool flycast2021LowEnd = false;
bool flycast2022 = false;

size_t fake_serialize_size()
{
    ++serialize_size_calls;
    return 8;
}

bool fake_unserialize(const void*, size_t)
{
    assert(audio_flushes == unserializes + 1);
    assert(video_barriers == unserializes + 1);
    ++unserializes;
    return true;
}

bool set_eject(bool value)
{
    if (value) {
        assert(audio_flushes == unserializes + 1);
        assert(video_barriers == unserializes + 1);
        assert(disk_step == 0);
        disk_step = 1;
    } else {
        assert(disk_step == 4);
        disk_step = 5;
    }
    ejected = value;
    return true;
}

bool get_eject() { return ejected; }
unsigned get_index() { return 0; }
bool set_index(unsigned index) { assert(index == 1 && disk_step == 3); disk_step = 4; return true; }
unsigned get_count() { return 1; }
bool replace_index(unsigned index, const retro_game_info* info)
{
    assert(index == 1 && info && info->path && disk_step == 2);
    disk_step = 3;
    return true;
}
bool add_index() { assert(disk_step == 1); disk_step = 2; return true; }
}

bool audio_flush() { ++audio_flushes; return true; }
void video_synchronize() { ++video_barriers; }
void achievements_change_media(const char*) {}
bool isFlycast2021() { return flycast2021 || flycast2021LowEnd || flycast2022; }
bool isFlycast2021LowEnd() { return flycast2021LowEnd; }
bool isFlycast2022() { return flycast2022; }

std::string replace(std::string& source, const std::string& from,
                    const std::string& to)
{
    std::string result = source;
    const size_t position = result.find(from);
    if (position != std::string::npos)
        result.replace(position, from.size(), to);
    return result;
}

int main()
{
    main_thread_id = pthread_self();
    g_retro.retro_unserialize = fake_unserialize;

    const char* state_path = "transition-test.state";
    unsigned char state[16] = {};
    std::FILE* file = std::fopen(state_path, "wb");
    assert(file);
    assert(std::fwrite(state, 1, sizeof(state), file) == sizeof(state));
    std::fclose(file);
    assert(LoadState(state_path) == 0);
    assert(unserializes == 1);
    std::remove(state_path);

    // A core may report a dynamic serialization size. Loading must pass the
    // complete file to retro_unserialize() instead of rejecting a valid state
    // by comparing it with the current frame's size.
    g_retro.retro_serialize_size = fake_serialize_size;
    file = std::fopen(state_path, "wb");
    assert(file);
    assert(std::fwrite(state, 1, sizeof(state), file) == sizeof(state));
    std::fclose(file);
    assert(StartLoadStateAsync(state_path, 0, true));
    while (LoadStateProgress() < 100)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    PumpLoadStateAsync();
    assert(lastLoadSaveStateDoneOk);
    assert(input_slot_memory_load_done);
    assert(unserializes == 2);
    assert(serialize_size_calls == 0);
    ShutdownLoadStateAsync();
    std::remove(state_path);

    // A missing auto-state is a normal first-run condition. It must be
    // completed synchronously without creating a worker or touching the core.
    input_slot_memory_load_done = false;
    input_slot_memory_load_requested = true;
    lastLoadSaveStateDoneOk = true;
    assert(!StartLoadStateAsync("missing-transition-test.state", 0, true));
    assert(!LoadStateAsyncBusy());
    assert(input_slot_memory_load_done);
    assert(!input_slot_memory_load_requested);
    assert(lastLoadSaveStateDoneOk);
    assert(LoadStateMissingAuto());
    assert(LoadStateStatusMessage() == "No auto state");
    assert(unserializes == 2);

    input_slot_memory_load_done = false;
    input_slot_memory_load_requested = true;
    assert(!StartLoadStateAsync("missing-transition-test.state", 1, false));
    assert(!LoadStateMissingAuto());
    assert(!lastLoadSaveStateDoneOk);
    assert(LoadStateStatusMessage() == "State file not found");

    flycast2021LowEnd = true;
    char *lowEndPath = createSavePath("/roms/test.cdi", "/saves");
    assert(std::string(lowEndPath) == "/saves/test.fc2021le-rrstate.auto");
    std::free(lowEndPath);
    flycast2021LowEnd = false;

    flycast2022 = true;
    char *flycast2022Path = createSavePath("/roms/test.cdi", "/saves");
    assert(std::string(flycast2022Path) == "/saves/test.fc2022-rrstate.auto");
    std::free(flycast2022Path);
    flycast2022 = false;

    retro_disk_control_ext_callback disk = {};
    disk.set_eject_state = set_eject;
    disk.get_eject_state = get_eject;
    disk.get_image_index = get_index;
    disk.set_image_index = set_index;
    disk.get_num_images = get_count;
    disk.replace_image_index = replace_index;
    disk.add_image_index = add_index;
    rr_disk_control_set_ext(&disk);
    std::string error;
    assert(rr_disk_control_add_and_select("second.cdi", &error));
    assert(error.empty());
    assert(disk_step == 5);
    rr_disk_control_clear();
    return 0;
}
