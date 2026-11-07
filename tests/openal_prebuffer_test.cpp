#include "go2/prebuffer_policy.h"
#include <cassert>
int main(){
 using go2_audio_policy::prebufferReady;
 // 70.087 Hz DOS: four 629-frame buffers cannot reach the 2646-frame target.
 assert(!prebufferReady(629,2646,1,false));
 assert(!prebufferReady(1258,2646,2,false));
 assert(!prebufferReady(1887,2646,3,false));
 assert(prebufferReady(2516,2646,4,true));
 // Ordinary 60/50 Hz startup still follows the configured target.
 assert(!prebufferReady(2205,2646,3,false));
 assert(prebufferReady(2940,2646,4,true));
 assert(prebufferReady(2646,2646,3,false));
 assert(!prebufferReady(512,0,1,true));
 assert(prebufferReady(512,8820,4,true));
}
