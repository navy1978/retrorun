#pragma once

#include "libretro.h"
#include <string>

void rr_disk_control_set(const retro_disk_control_callback* callback);
void rr_disk_control_set_ext(const retro_disk_control_ext_callback* callback);
void rr_disk_control_clear();
bool rr_disk_control_available();
bool rr_disk_control_add_and_select(const std::string& path, std::string* error);
