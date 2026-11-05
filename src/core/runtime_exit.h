/* Stable RetroRun process-exit contract. Nonzero remains an error for legacy
 * launch scripts. Signals are reported separately by the session supervisor. */
#pragma once
enum rr_runtime_exit_code {
    RR_EXIT_OK = 0,
    RR_EXIT_ARGUMENTS = 64,
    RR_EXIT_CORE_LOAD = 65,
    RR_EXIT_CORE_ABI = 66,
    RR_EXIT_CONTENT_READ = 67,
    RR_EXIT_ARCHIVE = 68,
    RR_EXIT_CONTENT_REJECTED = 69,
    RR_EXIT_VIDEO = 70,
    RR_EXIT_AUDIO = 71,
    RR_EXIT_STATE = 72,
    RR_EXIT_CORE_INIT = 73,
    RR_EXIT_PLATFORM = 74,
    RR_EXIT_MEMORY = 75,
    RR_EXIT_RUNTIME = 76
};

#ifdef __cplusplus
// Fatal paths must not run destructors of partially initialized worker threads.
[[noreturn]] void rr_runtime_fail(int code);
#endif
