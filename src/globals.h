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
#include "menu/menu_manager.h"
#include "logger.h"
#include <map>


extern std::string release;

enum RETRORUN_CORE_TYPE
{
  RETRORUN_CORE_UNKNOWN = 0,
  RETRORUN_CORE_ATARI800,
  RETRORUN_CORE_MGBA,
  RETRORUN_CORE_PARALLEL_N64,
  RETRORUN_CORE_FLYCAST,

};


enum TateState {
    DISABLED,
    ENABLED,
    REVERSED,
    AUTO
};

enum Device
{
  P_M,
  V_MP,
  RG_503,
  RG_552,
  UNKNOWN
};

enum Resolution
{
  R_320_240,
  R_640_480,
  R_UNKNOWN
};

enum AnalogToDigital
{
  NONE,
  LEFT_ANALOG,
  RIGHT_ANALOG,
  LEFT_ANALOG_FORCED,
  RIGHT_ANALOG_FORCED,

};

extern Device device;
extern Resolution resolution;

extern AnalogToDigital analogToDigital;

struct Image
{
  unsigned int width;
  unsigned int height;
  unsigned int bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */
  unsigned char pixel_data[152 * 49 * 2 + 1];
};


struct CpuInfo
{
  std::string number_of_cpu;
  std::string cpu_name;
  std::string thread_per_cpu;
  
};

extern std::string gpu_name;

/*struct rrImg
{
  bigImg big;
  smallImg small;
};*/

extern RETRORUN_CORE_TYPE Retrorun_Core;
extern TateState tateState;

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
extern bool swapL1R1WithL2R2;
extern bool swapSticks;
extern bool audio_disabled;

extern float opt_aspect;
extern float aspect_ratio;
extern float game_aspect_ratio;
extern std::string romName;
extern std::string coreName;
extern std::string coreVersion;
extern bool coreReadZippedFiles;
extern std::string screenShotFolder;
extern std::vector<CpuInfo> cpu_info_list;

extern std::string status_message;
extern int deviceTypeSelected;

// Define a map to store controller descriptions and their corresponding IDs
extern std::map<unsigned, std::string> controllerMap;

extern Logger logger;

// get system env 
const char* getEnv( const char* tag) noexcept ;

std::vector<std::string> exec(const char* cmd) ;

// Cores
bool isFlycast();
bool isFlycast2021();
bool isParalleln64();
bool isSwanStation();
bool isMGBA();
bool isVBA();
bool isJaguar();
bool isDosBox();
bool isDosCore();
bool isBeetleVB();
bool isMame();
bool isPPSSPP();
bool isDuckStation();
//bool isPUAE();

// Devices
const char* getDeviceName() noexcept ;
bool isRG351M();
bool isRG351P();
bool isRG351V();
bool isRG351MP();
bool isRG552();
bool isRG503();
bool isTate();

extern bool processVideoInAnotherThread;


extern bool runLoopAtDeclaredfps;
extern int retrorun_audio_buffer;
extern int  new_retrorun_audio_buffer; // this is only need when we change the other one on the fly via menu
extern int retrorun_mouse_speed_factor;

extern float avgFps;

#define BLACK 0x0000 // U16 definition
#define RED 0xF800 // DONT USE THIS: IT MAKES THE MENU CRASHES IN CERTAIN CORES
#define GREEN 0x07E0
#define DARKGREEN 0x0408
#define WHITE 0xFFFF
#define GREY 0x8410
#define DARKGREY 0x7BEF
#define LIGHTGREY 0xBDF7
#define YELLOW 0xFFE0 // DONT USE THIS: IT MAKES THE MENU CRASHES IN CERTAIN CORES
#define BLUE 0x001F
#define CYAN 0x07FF
#define LIGHTCYAN 0x87FF
#define MAGENTA 0xF81F
#define ORANGE 0xFBE0
#define BROWN 0x79E0 // DONT USE THIS: IT MAKES THE MENU CRASHES IN CERTAIN CORES
#define PINK 0xF81F


// Define the menu
extern MenuManager menuManager;
extern int current_volume;
extern std::string retrorun_device_name;
extern bool firstTimeCorrectFrame;

