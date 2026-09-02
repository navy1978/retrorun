#pragma once

/*
retrorun - libretro frontend for Anbernic Devices
Copyright (C) 2020  OtherCrashOverride
Copyright (C) 2021-present  navy1978
*/

#include "globals.h"

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
        key != "flycast2022_internal_resolution" &&
        key != "parallel-n64-screensize")
        return;

    if (value == "320x240")
        currentResolution = R_320_240;
    else if (value == "640x480")
        currentResolution = R_640_480;
}
