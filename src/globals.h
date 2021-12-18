#pragma once
#include <string>

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
    UNKNOWN
};
extern Device device;

struct bigImg{
  unsigned int 	 width;
  unsigned int 	 height;
  unsigned int 	 bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */ 
  unsigned char	 pixel_data[304 * 98 * 2 + 1];
};

struct smallImg{
  unsigned int 	 width;
  unsigned int 	 height;
  unsigned int 	 bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */ 
  unsigned char  pixel_data[152 * 49 * 2 + 1];
};

struct rrImg{
  bigImg big;
  smallImg small;
};




extern RETRORUN_CORE_TYPE Retrorun_Core;
extern bool force_left_analog_stick;

extern float fps;
extern bool opt_triggers;
extern bool gpio_joypad;
extern float opt_aspect;
extern float aspect_ratio;
extern std::string romName;
extern std::string coreName;
extern std::string screenShotFolder;

