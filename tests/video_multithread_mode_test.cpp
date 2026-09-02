#include "video/video_multithread_mode.h"

#include <cassert>
#include <iostream>
#include <string>

namespace
{

using rr::VideoMultithreadMode;

void testParser()
{
    VideoMultithreadMode mode = VideoMultithreadMode::Auto;

    assert(rr::parseVideoMultithreadMode("auto", &mode));
    assert(mode == VideoMultithreadMode::Auto);
    assert(rr::parseVideoMultithreadMode("ENABLED", &mode));
    assert(mode == VideoMultithreadMode::Enabled);
    assert(rr::parseVideoMultithreadMode("true", &mode));
    assert(mode == VideoMultithreadMode::Enabled);
    assert(rr::parseVideoMultithreadMode("off", &mode));
    assert(mode == VideoMultithreadMode::Disabled);
    assert(rr::parseVideoMultithreadMode("0", &mode));
    assert(mode == VideoMultithreadMode::Disabled);
    assert(!rr::parseVideoMultithreadMode("sometimes", &mode));
    assert(!rr::parseVideoMultithreadMode("auto", nullptr));

    assert(std::string(rr::videoMultithreadModeName(
               VideoMultithreadMode::Auto)) == "auto");
    assert(std::string(rr::videoMultithreadModeName(
               VideoMultithreadMode::Disabled)) == "disabled");
    assert(std::string(rr::videoMultithreadModeName(
               VideoMultithreadMode::Enabled)) == "enabled");
}

void testResolution()
{
    // Unsupported devices never start the worker, even for explicit enabled.
    for (VideoMultithreadMode mode : {
             VideoMultithreadMode::Auto,
             VideoMultithreadMode::Disabled,
             VideoMultithreadMode::Enabled})
    {
        for (bool legacyForce : {false, true})
            for (bool automaticPath : {false, true})
                assert(!rr::resolveVideoMultithreadRequest(
                    mode, legacyForce, false, automaticPath));
    }

    // Auto retains both historical ways of requesting the worker.
    assert(!rr::resolveVideoMultithreadRequest(
        VideoMultithreadMode::Auto, false, true, false));
    assert(rr::resolveVideoMultithreadRequest(
        VideoMultithreadMode::Auto, true, true, false));
    assert(rr::resolveVideoMultithreadRequest(
        VideoMultithreadMode::Auto, false, true, true));

    // Explicit disabled/enabled must win over the legacy and automatic paths.
    assert(!rr::resolveVideoMultithreadRequest(
        VideoMultithreadMode::Disabled, true, true, true));
    assert(rr::resolveVideoMultithreadRequest(
        VideoMultithreadMode::Enabled, false, true, false));
}

} // namespace

int main()
{
    testParser();
    testResolution();
    std::cout << "video multithread mode tests passed\n";
    return 0;
}
