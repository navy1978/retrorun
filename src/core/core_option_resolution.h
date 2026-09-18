#pragma once

/*
retrorun - libretro frontend for Anbernic Devices
Copyright (C) 2020  OtherCrashOverride
Copyright (C) 2021-present  navy1978
*/

#include "globals.h"
#include "libretro.h"

#include <string_view>

// Keep RetroRun's video geometry in sync with resolution options exposed by
// supported cores. AmberELEC renames the Flycast 2022 Low-End option namespace
// from reicast_* to flycast2022_* when packaging the core.
inline void updateResolutionFromCoreOption(std::string_view key,
                                           std::string_view value,
                                           Resolution &currentResolution) noexcept
{
    if (key != "flycast_internal_resolution" &&
        key != "flycast2021_internal_resolution" &&
        key != "flycast2021le_internal_resolution" &&
        key != "flycast2022_internal_resolution" &&
        key != "parallel-n64-screensize")
        return;

    if (value == "320x240")
        currentResolution = R_320_240;
    else if (value == "640x480")
        currentResolution = R_640_480;
}

// Keep startup and runtime AV updates consistent. Flycast reports square
// maximum dimensions even when the configured framebuffer is 640x480.
inline void applyConfiguredResolution(retro_game_geometry &geometry,
                                      Resolution resolution,
                                      bool preserve640Geometry) noexcept
{
    if (resolution == R_320_240)
    {
        geometry.base_width = geometry.max_width = 320;
        geometry.base_height = geometry.max_height = 240;
    }
    else if (resolution == R_640_480 && !preserve640Geometry)
    {
        geometry.base_width = geometry.max_width = 640;
        geometry.base_height = geometry.max_height = 480;
    }
}
