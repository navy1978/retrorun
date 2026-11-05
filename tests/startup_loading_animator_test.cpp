#include "../src/video/startup_loading_animator.h"
#include <cassert>
#include <future>
#include <stdexcept>
using namespace std::chrono_literals;
int main(){
 StartupLoadingAnimator animator;
 std::atomic<unsigned> frames{0};std::promise<void> third;
 animator.start([&]{if(++frames==3)third.set_value();},10ms);
 assert(third.get_future().wait_for(1s)==std::future_status::ready);
 animator.stop();const auto stopped=frames.load();std::this_thread::sleep_for(40ms);assert(frames==stopped);
 // Releasing video resources must wait for an in-flight draw, not just set a flag.
 std::promise<void> entered,release;auto ready=release.get_future().share();
 animator.start([&]{entered.set_value();ready.wait();},10ms);entered.get_future().wait();
 auto join=std::async(std::launch::async,[&]{animator.stop();});
 assert(join.wait_for(40ms)==std::future_status::timeout);release.set_value();join.get();
 animator.start([]{throw std::runtime_error("Cosmetic draw failure");},1ms);
 for(unsigned i=0;i<100 && !animator.failed();++i)std::this_thread::sleep_for(1ms);
 animator.stop();assert(animator.failed());
}
