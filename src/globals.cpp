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
#include "globals.h"
#include <string>



std::string release= "1.4.0";

RETRORUN_CORE_TYPE Retrorun_Core = RETRORUN_CORE_UNKNOWN;
Device device = UNKNOWN;
bool force_left_analog_stick = true;

bool opt_triggers = false;
bool gpio_joypad = false;
float opt_aspect = 0.0f;
float aspect_ratio = 0.0f;

std::string romName;
std::string coreName;

std::string screenShotFolder;

float fps=0.0f;
int retrorunLoopCounter =0;
int retrorunLoopSkip =10; // 10 ?


bool processVideoInAnotherThread= true;
int waitMSecForVideoInAnotherThread= 0;

bool processAudioInAnotherThread= true;
int waitMSecForAudioInAnotherThread= 0;

bool runLoopAt60fps= true;


bool isFlycast(){
    return coreName == "Flycast";
}


bool isParalleln64(){
    return coreName == "ParaLLEl N64";
}




