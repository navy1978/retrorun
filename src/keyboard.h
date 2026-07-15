#pragma once

/*
retrorun - libretro keyboard dispatcher
Copyright (C) 2021-present navy1978
*/

#include "platform.h"
#include "libretro.h"

// Register the core's keyboard callback (called from core_environment when
// the core issues RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK).
void rr_keyboard_set_callback(retro_keyboard_event_t cb);

// Clear the registered callback (call on core unload).
void rr_keyboard_clear_callback();

// Returns true if a core has registered a keyboard callback.
bool rr_keyboard_has_callback();

// Backend-independent on-screen keyboard, driven by frontend actions.
bool rr_keyboard_virtual_visible();
void rr_keyboard_virtual_open();
void rr_keyboard_virtual_close();
void rr_keyboard_virtual_input(bool up, bool down, bool left, bool right,
                               bool accept, bool cancel, bool shift);
void rr_keyboard_virtual_render(rr_surface_t* surface, int width, int height);
