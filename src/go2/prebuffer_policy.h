#pragma once
#include <cstddef>
#include <cstdint>
namespace go2_audio_policy {
// A variable core batch may fill every OpenAL buffer before reaching the time
// target. Start then: no buffer can be reclaimed while playback is stopped.
inline bool prebufferReady(std::uint64_t frames,std::uint64_t target,std::size_t queued,bool exhausted){
    return queued>=2 && (frames>=target || exhausted);
}
}
