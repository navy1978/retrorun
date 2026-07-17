#pragma once

#include "platform.h"

#include <string>

void decoration_init(const char* content_path);
void decoration_shutdown();
bool decoration_enabled();
void decoration_set_enabled(bool enabled);
rr_surface_t* decoration_surface();
rr_surface_t* decoration_background_surface();
bool decoration_game_viewport(int* x, int* y, int* width, int* height);
std::string decoration_directory();
std::string decoration_source_label();
void decoration_reload();
