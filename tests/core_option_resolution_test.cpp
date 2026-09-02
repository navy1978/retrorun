#include "core_option_resolution.h"

#include <cassert>

int main()
{
    Resolution resolution = R_UNKNOWN;
    updateResolutionFromCoreOption("flycast2022_internal_resolution",
                                   "320x240", resolution);
    assert(resolution == R_320_240);

    updateResolutionFromCoreOption("flycast_internal_resolution",
                                   "640x480", resolution);
    assert(resolution == R_640_480);
    updateResolutionFromCoreOption("flycast2021_internal_resolution",
                                   "320x240", resolution);
    assert(resolution == R_320_240);
    updateResolutionFromCoreOption("parallel-n64-screensize",
                                   "640x480", resolution);
    assert(resolution == R_640_480);

    updateResolutionFromCoreOption("unrelated_internal_resolution",
                                   "320x240", resolution);
    assert(resolution == R_640_480);
    updateResolutionFromCoreOption("flycast2022_internal_resolution",
                                   "1024x768", resolution);
    assert(resolution == R_640_480);
    return 0;
}
