#include "input/async_input_snapshot.h"
#include <cassert>
#include <thread>
int main() {
    rr::AsyncInputSnapshot state;
    assert(!state.joy(0,0)); assert(!state.analog(0,0,0));
    state.publish(0,0x8009,-32768,32767,123,-456);
    assert(state.joy(0,0)==1 && state.joy(0,1)==0 && state.joy(0,3)==1);
    assert(uint16_t(state.joy(0,256))==0x8009);
    assert(state.analog(0,0,0)==-32768 && state.analog(0,0,1)==32767);
    assert(state.analog(0,1,0)==123 && state.analog(0,1,1)==-456);
    assert(!state.joy(4,0)&&!state.joy(0,17)&&!state.analog(0,2,0)&&!state.analog(0,0,2));
    std::atomic<bool> done{false};
    std::thread worker([&]{while(!done.load()) {auto joy=uint16_t(state.joy(1,256));assert(joy==0||joy==0x5555||joy==0xaaaa);(void)state.analog(1,0,1);}});
    for(unsigned i=0;i<100000;++i) state.publish(1,i%2?0x5555:0xaaaa,int16_t(i),int16_t(-i),0,0);
    done.store(true);worker.join();
    state.publish(0,0,0,0,0,0);assert(!state.joy(0,256)&&!state.analog(0,0,1));
}
