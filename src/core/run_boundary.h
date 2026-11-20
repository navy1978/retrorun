// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

namespace rr {
#if defined(__aarch64__) && defined(__GNUC__)
// Some legacy ARM64 core assembly uses v8-v15 without restoring their low
// 64 bits. Keep frontend timing/math values safe across the external callback.
// The clobbers make the compiler save/restore the ABI-preserved d8-d15 here;
// noinline keeps that protection at the callback boundary under LTO.
__attribute__((noinline)) inline void runCoreFrame(void (*run)()) {
    run();
    asm volatile("" ::: "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "memory");
}
#else
inline void runCoreFrame(void (*run)()) { run(); }
#endif
}
