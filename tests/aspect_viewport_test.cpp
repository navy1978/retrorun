#include "video/aspect_viewport.h"
#include <cassert>
#include <limits>
int main(){
 const auto dosMenu=rr::fitAspect(640,480,768.f/528.f);assert(dosMenu.x==0 && dosMenu.y==20 && dosMenu.width==640 && dosMenu.height==440);
 const auto nes=rr::fitAspect(640,480,rr::resolvedAspect(0,8.f/7.f,256,224));
 assert(nes.width==549 && nes.height==480 && nes.x==45 && nes.y==0);
 const auto md=rr::fitAspect(640,480,rr::resolvedAspect(0,10.f/7.f,320,224));
 assert(md.width==640 && md.height==448 && md.x==0 && md.y==16);
 const auto mdNtsc=rr::fitAspect(640,480,1.306f);assert(mdNtsc.width==627&&mdNtsc.height==480&&mdNtsc.y==0);
 const auto dar=rr::fitAspect(640,480,rr::resolvedAspect(0,4.f/3.f,256,224));assert(dar.width==640&&dar.height==480);
 const auto forced=rr::fitAspect(640,480,rr::resolvedAspect(16.f/9.f,4.f/3.f,320,224));assert(forced.height==360&&forced.y==60);
 assert(rr::resolvedAspect(0,0,320,224)==320.f/224.f);
 assert(rr::resolvedAspect(0,std::numeric_limits<float>::quiet_NaN(),256,224)==256.f/224.f);
 for(float ar:{1.f,8.f/7.f,4.f/3.f,10.f/7.f,16.f/9.f,2.f})for(auto size:{640,854,1280}){
  auto v=rr::fitAspect(size,480,ar);assert(v.x>=0&&v.y>=0&&v.width+v.x<=size&&v.height+v.y<=480);
  assert(std::abs(float(v.width)/v.height-ar)<.01f);
 }
}
