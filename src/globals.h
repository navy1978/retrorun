#pragma once

enum RETRORUN_CORE_TYPE
{
    RETRORUN_CORE_UNKNOWN = 0,
    RETRORUN_CORE_ATARI800,
    RETRORUN_CORE_MGBA,
    RETRORUN_CORE_PARALLEL_N64,

};

extern RETRORUN_CORE_TYPE Retrorun_Core;
extern bool Retrorun_UseAnalogStick;

extern bool opt_triggers;
extern float opt_aspect;
extern float aspect_ratio;
