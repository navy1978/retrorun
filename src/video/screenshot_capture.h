#pragma once
#include "platform.h"
#include <algorithm>
#include <cmath>
#include <cstring>

// Existing platform conversion/PNG writer, with bounded buffers only during a
// capture. CPU rotation is applied to the small RGB snapshot, never the game
// surface; physical panel rotation does not belong in an artwork file.
inline bool captureSurfacePng(rr_display_t* display,rr_surface_t* source,
    int sx,int sy,int width,int height,const char* path,int maxWidth,int maxHeight,
    rr_rotation_t rotation){
    if(!display || !source || !path || !*path || width<=0 || height<=0)return false;
    const bool sideways=rotation==RR_ROTATION_DEGREES_90 || rotation==RR_ROTATION_DEGREES_270;
    const double scale=maxWidth>0 && maxHeight>0 ? std::min({1.0,double(maxWidth)/(sideways?height:width),double(maxHeight)/(sideways?width:height)}):1.0;
    const int sw=std::max(1,int(width*scale)),sh=std::max(1,int(height*scale));
    auto* image=rr_surface_create(display,sw,sh,RR_PIXEL_FORMAT_RGB888);if(!image)return false;
    rr_surface_blit_pixels(source,sx,sy,width,height,image,0,0,sw,sh);
    rr_surface_t* rotated=nullptr;
    if(rotation!=RR_ROTATION_DEGREES_0){
        const int dw=sideways?sh:sw,dh=sideways?sw:sh;
        rotated=rr_surface_create(display,dw,dh,RR_PIXEL_FORMAT_RGB888);
        if(!rotated){rr_surface_destroy(image);return false;}
        auto* src=static_cast<unsigned char*>(rr_surface_map(image));
        auto* dst=static_cast<unsigned char*>(rr_surface_map(rotated));
        if(!src || !dst){if(src)rr_surface_unmap(image);if(dst)rr_surface_unmap(rotated);rr_surface_destroy(rotated);rr_surface_destroy(image);return false;}
        const int ss=rr_surface_stride_get(image),ds=rr_surface_stride_get(rotated);
        for(int y=0;y<sh;++y)for(int x=0;x<sw;++x){
            int dx=x,dy=y;
            if(rotation==RR_ROTATION_DEGREES_90){dx=sh-1-y;dy=x;}
            else if(rotation==RR_ROTATION_DEGREES_270){dx=y;dy=sw-1-x;}
            else if(rotation==RR_ROTATION_DEGREES_180){dx=sw-1-x;dy=sh-1-y;}
            std::memcpy(dst+dy*ds+dx*3,src+y*ss+x*3,3);
        }
        rr_surface_unmap(image);rr_surface_unmap(rotated);
    }
    const bool ok=rr_surface_save_as_png(rotated?rotated:image,path)==0;
    if(rotated)rr_surface_destroy(rotated);
    rr_surface_destroy(image);return ok;
}
