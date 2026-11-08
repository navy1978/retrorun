#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
namespace rr {
struct AspectViewport {int x=0,y=0,width=0,height=0;};
inline bool validAspect(float value){
    // Release uses -ffast-math: test IEEE bits rather than isfinite(), which
    // the compiler may fold to true under that option.
    std::uint32_t bits;std::memcpy(&bits,&value,sizeof bits);
    return !(bits&0x80000000U) && (bits&0x7fffffffU) && (bits&0x7f800000U)!=0x7f800000U;
}
inline float resolvedAspect(float requested,float core,int width,int height){
    if(validAspect(requested))return requested;
    if(validAspect(core))return core;
    return width>0 && height>0?float(width)/height:4.f/3.f;
}
inline AspectViewport fitAspect(int width,int height,float aspect){
    if(width<=0 || height<=0)return {};
    aspect=resolvedAspect(aspect,0,width,height);
    AspectViewport r{0,0,width,height};
    if(aspect<float(width)/height)r.width=std::clamp(int(std::lround(height*aspect)),1,width);
    else r.height=std::clamp(int(std::lround(width/aspect)),1,height);
    r.x=(width-r.width)/2;r.y=(height-r.height)/2;return r;
}
// Native landscape-panel fitting applies to portrait games too. In particular,
// a 3:4 game must not fall through a legacy reciprocal-aspect equality gap.
inline bool fitNativePanel(bool nativePanel,bool tate,int displayWidth,int displayHeight,
                           float requested,float core,int frameWidth,int frameHeight,
                           AspectViewport& result){
    if(!nativePanel || tate)return false;
    result=fitAspect(displayWidth,displayHeight,resolvedAspect(requested,core,frameWidth,frameHeight));
    return true;
}
}
