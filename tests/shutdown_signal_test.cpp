#include "shutdown_signal.h"
#include <cassert>
#include <unistd.h>
int main() {
    struct sigaction before{},after{};
    sigaction(SIGTERM,nullptr,&before);
    {
        rr_shutdown::Signals signals;
        assert(!rr_shutdown::requested);
        assert(kill(getpid(),SIGTERM)==0);
        assert(rr_shutdown::requested);
        rr_shutdown::requested=0;
        assert(kill(getpid(),SIGINT)==0);
        assert(rr_shutdown::requested);
    }
    sigaction(SIGTERM,nullptr,&after);
    assert(before.sa_handler==after.sa_handler);
    { rr_shutdown::Signals signals; assert(!rr_shutdown::requested); }
}
