#include "disk_control.h"
#include "achievements.h"
#include "audio.h"
#include "video.h"

#include <cstring>

namespace {
retro_disk_control_ext_callback disk = {};
bool available = false;
}

void rr_disk_control_set(const retro_disk_control_callback* cb) {
    std::memset(&disk, 0, sizeof(disk));
    if (!cb) { available = false; return; }
    disk.set_eject_state = cb->set_eject_state;
    disk.get_eject_state = cb->get_eject_state;
    disk.get_image_index = cb->get_image_index;
    disk.set_image_index = cb->set_image_index;
    disk.get_num_images = cb->get_num_images;
    disk.replace_image_index = cb->replace_image_index;
    disk.add_image_index = cb->add_image_index;
    available = disk.set_eject_state && disk.set_image_index && disk.get_num_images;
}

void rr_disk_control_set_ext(const retro_disk_control_ext_callback* cb) {
    if (cb) disk = *cb; else std::memset(&disk, 0, sizeof(disk));
    available = cb && disk.set_eject_state && disk.set_image_index && disk.get_num_images;
}

void rr_disk_control_clear() { std::memset(&disk, 0, sizeof(disk)); available = false; }
bool rr_disk_control_available() { return available; }

bool rr_disk_control_add_and_select(const std::string& path, std::string* error) {
    auto fail = [&](const char* message) { if (error) *error = message; return false; };
    if (!available) return fail("Core does not support disk control");
    if (!disk.replace_image_index || !disk.add_image_index)
        return fail("Core cannot add disk images");
    audio_flush();
    video_synchronize();
    const bool was_ejected = disk.get_eject_state && disk.get_eject_state();
    if (!was_ejected && !disk.set_eject_state(true)) return fail("Cannot eject disk");
    const unsigned index = disk.get_num_images();
    if (!disk.add_image_index()) {
        if (!was_ejected) disk.set_eject_state(false);
        return fail("Cannot add disk slot");
    }
    const retro_game_info info = {path.c_str(), nullptr, 0, nullptr};
    if (!disk.replace_image_index(index, &info) || !disk.set_image_index(index)) {
        disk.replace_image_index(index, nullptr);
        if (!was_ejected) disk.set_eject_state(false);
        return fail("Core rejected selected image");
    }
    if (!was_ejected && !disk.set_eject_state(false)) return fail("Cannot insert disk");
    achievements_change_media(path.c_str());
    if (error) error->clear();
    return true;
}
