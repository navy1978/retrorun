#pragma once

/*
retrorun-go2 - libretro frontend for the ODROID-GO Advance
Copyright (C) 2020-2022  OtherCrashOverride - navy1978

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

#include <stdlib.h>

void audio_init(int freq);
void audio_deinit();
void core_audio_sample(int16_t left, int16_t right);
size_t core_audio_sample_batch(const int16_t * data, size_t frames);
