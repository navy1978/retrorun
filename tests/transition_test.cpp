#include "core_loader.h"
#include "disk_control.h"
#include "logger.h"
#include "savestate.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <string>

Logger logger(Logger::ERR);
RetroCore g_retro = {};
pthread_t main_thread_id;
bool input_slot_memory_save_done = false;
double lastLoadSaveStateDoneTime = 0.0;

namespace {
int audio_flushes = 0;
int video_barriers = 0;
int unserializes = 0;
int disk_step = 0;
bool ejected = false;

bool fake_unserialize(const void*, size_t)
{
    assert(audio_flushes == 1);
    assert(video_barriers == 1);
    ++unserializes;
    return true;
}

bool set_eject(bool value)
{
    if (value) {
        assert(audio_flushes == 2);
        assert(video_barriers == 2);
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
bool isFlycast2021() { return false; }

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
