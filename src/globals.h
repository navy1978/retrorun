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
#pragma once
#include <string>

extern std::string release;

enum RETRORUN_CORE_TYPE
{
  RETRORUN_CORE_UNKNOWN = 0,
  RETRORUN_CORE_ATARI800,
  RETRORUN_CORE_MGBA,
  RETRORUN_CORE_PARALLEL_N64,
  RETRORUN_CORE_FLYCAST,

};

enum Device
{
  P_M,
  V_MP,
  RG_552,
  UNKNOWN
};

enum Resolution
{
  R_320_240,
  R_640_480,
  R_UNKNOWN
};

extern Device device;
extern Resolution resolution;

struct bigImg
{
  unsigned int width;
  unsigned int height;
  unsigned int bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */
  unsigned char pixel_data[304 * 98 * 2 + 1];
};

struct smallImg
{
  unsigned int width;
  unsigned int height;
  unsigned int bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */
  unsigned char pixel_data[152 * 49 * 2 + 1];
};

struct rrImg
{
  bigImg big;
  smallImg small;
};

extern RETRORUN_CORE_TYPE Retrorun_Core;
extern bool force_left_analog_stick;

extern float originalFps;
extern float fps;
extern float newFps;
extern int retrorunLoopCounter;
extern int retrorunLoopSkip;
extern int frameCounter;
extern int frameCounterSkip;
extern int audioCounter;
extern int audioCounterSkip;

extern bool opt_triggers;
extern bool gpio_joypad;
extern bool adaptiveFps;

extern float opt_aspect;
extern float aspect_ratio;
extern std::string romName;
extern std::string coreName;
extern std::string screenShotFolder;

bool isFlycast();
bool isParalleln64();
bool isSwanStation();
bool isMGBA();
bool isJaguar();
extern bool processVideoInAnotherThread;
extern int waitMSecForVideoInAnotherThread;
extern bool enableSwitchVideoSync;

extern bool processAudioInAnotherThread;
extern int waitMSecForAudioInAnotherThread;
extern bool enableSwitchAudioSync;

extern bool runLoopAt60fps;
