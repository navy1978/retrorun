#pragma once
#include <signal.h>

// The handler only publishes a signal-safe request. Saving SRAM and tearing
// down audio/video/core state remains on the normal main-thread exit path.
namespace rr_shutdown {
inline volatile sig_atomic_t requested=0;
inline void request(int) { requested=1; }
class Signals {
public:
    Signals() {
        requested=0;
        struct sigaction action{};
        action.sa_handler=request;
        sigemptyset(&action.sa_mask);
        action.sa_flags=SA_RESTART;
        term_=sigaction(SIGTERM,&action,&oldTerm_)==0;
        interrupt_=sigaction(SIGINT,&action,&oldInterrupt_)==0;
    }
    ~Signals() {
        if(term_)sigaction(SIGTERM,&oldTerm_,nullptr);
        if(interrupt_)sigaction(SIGINT,&oldInterrupt_,nullptr);
    }
    Signals(const Signals&)=delete;
    Signals& operator=(const Signals&)=delete;
private:
    struct sigaction oldTerm_{},oldInterrupt_{};
    bool term_=false,interrupt_=false;
};
}
