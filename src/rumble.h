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
#pragma once
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include "libretro.h"
#include <iostream>


// for rumble event
extern std::string DEVICE_PATH;

// for pwm rumble
extern std::string PWM_RUMBLE_PATH;
#define PWM_RUMBLE_ON "100000\n"
#define PWM_RUMBLE_OFF "1000000\n"

extern bool pwm; // pwm or event
extern bool disableRumble; // to disable rumble


bool retrorun_input_set_rumble(unsigned port, enum retro_rumble_effect effect, uint16_t strength);

