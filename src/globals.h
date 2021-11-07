#pragma once
#include <string>

enum RETRORUN_CORE_TYPE
{
    RETRORUN_CORE_UNKNOWN = 0,
    RETRORUN_CORE_ATARI800,
    RETRORUN_CORE_MGBA,
    RETRORUN_CORE_PARALLEL_N64,

};


enum Device
{
    P_M,
    V_MP,
    UNKNOWN
};
extern Device device;


extern RETRORUN_CORE_TYPE Retrorun_Core;
extern bool force_left_analog_stick;

extern float fps;
extern bool opt_triggers;
extern bool gpio_joypad;
extern float opt_aspect;
extern float aspect_ratio;
extern std::string romName;
extern std::string screenShotFolder;

