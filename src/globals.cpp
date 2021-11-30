#include "globals.h"
#include <string>



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

