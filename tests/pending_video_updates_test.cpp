#include "core/pending_video_updates.h"
#include <cassert>
#include <thread>
int main(){
 rr::PendingVideoUpdates pending;retro_system_av_info av{},out{};retro_game_geometry geometry{},got{};
 av.geometry={256,192,348,240,1.52381f};av.timing={60,48000};geometry={320,224,9999,9999,1.306f};
 pending.stageAv(av);assert(pending.stageGeometry(geometry));assert(pending.takeAv(&out));
 assert(out.geometry.base_width==320&&out.geometry.max_width==348&&out.geometry.max_height==240);
 assert(out.geometry.aspect_ratio==1.306f&&out.timing.fps==60&&out.timing.sample_rate==48000);
 assert(!pending.takeGeometry(&got));
 assert(pending.stageGeometry(geometry));pending.stageAv(av);assert(!pending.takeGeometry(&got));assert(pending.takeAv(&out)&&out.geometry.base_width==256);
 assert(!pending.stageGeometry({0,224,0,0,1.3f}));
 assert(pending.stageGeometry(geometry));assert(pending.takeGeometry(&got)&&got.aspect_ratio==1.306f);assert(!pending.takeGeometry(&got));
 std::thread producer([&]{for(unsigned n=1;n<=2000;++n)assert(pending.stageGeometry({n,n+1,0,0,1.3f}));});
 for(unsigned n=0;n<2000;++n)if(pending.takeGeometry(&got))assert(got.base_height==got.base_width+1);
 producer.join();if(pending.takeGeometry(&got))assert(got.base_height==got.base_width+1);
 pending.stageAv(av);pending.stageGeometry(geometry);pending.clear();assert(!pending.takeAv(&out)&&!pending.takeGeometry(&got));
}
