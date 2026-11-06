#pragma once
// Opt-in aggregate diagnostics. No clocks/syscalls/logging in callback hot paths.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <thread>
namespace rr_input_trace {
enum Stage { Loop, RunEnter, RunExit, Evdev, EvdevPublish, Update, Poll, State, NonzeroState, Framebuffer, Count };
struct Counter { std::atomic<unsigned long long> count{0}, tid{0}; };
inline Counter counters[Count];
inline const bool enabled = [] { const char* value=std::getenv("RR_INPUT_TRACE"); return value && value[0]=='1'; }();
inline void hit(Stage stage) {
    if (!enabled) return;
    static thread_local const auto tid=static_cast<unsigned long long>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
    counters[stage].count.fetch_add(1,std::memory_order_relaxed);
    counters[stage].tid.store(tid,std::memory_order_relaxed);
}
inline void report() {
    if (!enabled) return;
    using Clock=std::chrono::steady_clock;
    static auto last=Clock::now();
    const auto now=Clock::now();
    if (now-last<std::chrono::seconds(2)) return;
    const double seconds=std::chrono::duration<double>(now-last).count(); last=now;
    constexpr const char* names[]={"loop","run_enter","run_exit","evdev","evdev_publish","update","poll","state","nonzero_state","framebuffer"};
    std::fprintf(stderr,"INPUT_TRACE dt=%.3f",seconds);
    for (unsigned i=0;i<Count;++i) std::fprintf(stderr," %s=%llu@%llx",names[i],counters[i].count.exchange(0,std::memory_order_relaxed),counters[i].tid.load(std::memory_order_relaxed));
    std::fputc('\n',stderr);
}
}
