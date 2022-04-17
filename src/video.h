#pragma once

/*
retrorun-go2 - libretro frontend for the ODROID-GO Advance
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
#include <stdlib.h>


extern uint32_t color_format;
extern bool isOpenGL;
extern int GLContextMajor;
extern int GLContextMinor;
extern int hasStencil;
extern bool screenshot_requested;
extern bool pause_requested;

void video_configure(const struct retro_game_geometry* geom);
void video_deinit();
uintptr_t core_video_get_current_framebuffer();
void core_video_refresh(const void * data, unsigned width, unsigned height, size_t pitch);

