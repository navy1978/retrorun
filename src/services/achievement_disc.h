#pragma once

#include "rc_hash.h"
#include <atomic>
#include <string>

namespace rr { namespace achievements {
// Each iterator/track owns its reader state; no process-global rcheevos hooks.
rc_hash_cdreader_t disc_reader();
bool uses_disc_reader(const std::string& path);
struct DiscHashResult {
    std::string hash;
    std::string error;
};
DiscHashResult hash_disc(const std::string& path, uint32_t console,
                         const std::atomic<bool>& cancel);
}}
