#pragma once

/*
retrorun - libretro frontend for Anbernic Devices
Copyright (C) 2020  OtherCrashOverride
Copyright (C) 2021-present  navy1978

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

#include <stdint.h>
#include <stdbool.h>
#include <go2/input.h>
#include <map>


extern bool input_exit_requested;
extern bool input_exit_requested_firstTime;
extern bool input_fps_requested;
extern bool input_info_requested; // L3+R3
extern bool input_info_requested_alternative; // SELECT +X
extern bool input_clean_screen;
extern bool input_reset_requested;
extern bool input_ffwd_requested;
extern bool input_pause_requested;
extern bool input_credits_requested;
extern bool input_message;
extern bool elable_key_log;
extern bool input_slot_memory_plus_requested;
extern bool input_slot_memory_minus_requested;
extern bool input_slot_memory_load_requested;
extern bool input_slot_memory_save_requested;
extern bool input_slot_memory_load_done;
extern bool input_slot_memory_save_done;
extern bool input_slot_memory_reset_done;

extern double lastScreenhotrequestTime;
extern double lastLoadSaveStateRequestTime;
extern double lastLoadSaveStateDoneTime;
extern bool lastLoadSaveStateDoneOk;

void input_gamepad_read();
go2_input_state_t* input_gampad_current_get();
void core_input_poll(void);
int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id);
void applyButtonRemapping();
