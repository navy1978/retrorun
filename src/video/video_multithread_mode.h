#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace rr
{

// Auto preserves RetroRun's historical behaviour: an explicit legacy
// retrorun_force_video_multithread=true request, plus the automatic
// RG552/Flycast-2021-family compatibility path.
enum class VideoMultithreadMode
{
    Auto,
    Disabled,
    Enabled
};

inline const char *videoMultithreadModeName(VideoMultithreadMode mode)
{
    switch (mode)
    {
    case VideoMultithreadMode::Disabled:
        return "disabled";
    case VideoMultithreadMode::Enabled:
        return "enabled";
    case VideoMultithreadMode::Auto:
    default:
        return "auto";
    }
}

inline bool parseVideoMultithreadMode(
    const std::string &value, VideoMultithreadMode *mode)
{
    if (mode == nullptr)
        return false;

    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });

    if (normalized == "auto")
        *mode = VideoMultithreadMode::Auto;
    else if (normalized == "enabled" || normalized == "on" ||
             normalized == "true" || normalized == "1")
        *mode = VideoMultithreadMode::Enabled;
    else if (normalized == "disabled" || normalized == "off" ||
             normalized == "false" || normalized == "0")
        *mode = VideoMultithreadMode::Disabled;
    else
        return false;

    return true;
}

inline bool resolveVideoMultithreadRequest(
    VideoMultithreadMode mode, bool legacyForce, bool deviceSupported,
    bool automaticCompatibilityPath)
{
    if (!deviceSupported)
        return false;

    switch (mode)
    {
    case VideoMultithreadMode::Disabled:
        return false;
    case VideoMultithreadMode::Enabled:
        return true;
    case VideoMultithreadMode::Auto:
    default:
        return legacyForce || automaticCompatibilityPath;
    }
}

} // namespace rr
