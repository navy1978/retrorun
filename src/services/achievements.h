#pragma once

#include "platform.h"
#include <string>

// Minimal RetroAchievements lifecycle. Configuration is read from conf_map:
//   retrorun_achievements_enabled = true
//   retrorun_achievements_username = name
//   retrorun_achievements_token = token
// A password may be used instead of a token, but storing a token is preferred.
void achievements_init(const char* content_path);
void achievements_frame();
void achievements_idle();
void achievements_reset();
void achievements_change_media(const char* content_path);
void achievements_shutdown();
bool achievements_enabled();
void achievements_set_enabled(bool value, const char* content_path);
std::string achievements_status_label();
std::string achievements_username_label();
void achievements_edit_username(const char* content_path);
void achievements_edit_password(const char* content_path);
bool achievements_active();
bool achievements_notification_visible();
void achievements_render_notification(rr_surface_t* surface);
bool achievements_view_visible();
void achievements_view_open();
void achievements_view_close();
void achievements_view_input(bool up, bool down, bool left, bool right,
                             bool accept, bool cancel);
void achievements_view_render(rr_surface_t* surface, int width, int height);
struct retro_memory_map;
void achievements_set_memory_map(const retro_memory_map* map);
