// Scalar curved CRT reference before packed interpolation optimization.
#pragma once
#include "video/go2_scanlines.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <new>

namespace go2_crt_scalar {
// CPU inverse warp in source-frame coordinates. Same radial coefficient as the
// GO2 GLES CRT shader; RGA still owns final viewport/scaling/rotation.
class Plan {
    struct Pixel {std::uint32_t offset;std::uint16_t gain[3];std::uint8_t fx,fy,edge;};
    std::vector<Pixel> pixels;
    unsigned width_=0,height_=0,bpp_=0;
    std::size_t pitch_=0;
    bool linear_=false,ready_=false;
public:
    bool prepare(unsigned width,unsigned height,std::size_t pitch,unsigned bpp,bool linear) {
        if(width_==width && height_==height && pitch_==pitch && bpp_==bpp && linear_==linear)return ready_;
        width_=width;height_=height;pitch_=pitch;bpp_=bpp;linear_=linear;ready_=false;
        pixels.clear();
        constexpr std::size_t budget=16*1024*1024;
        if(!width || !height || (bpp!=2 && bpp!=3 && bpp!=4) ||
           width>4096 || height>4096 || std::size_t(width)*height>budget/sizeof(Pixel) ||
           pitch<std::size_t(width)*bpp || pitch>UINT32_MAX/height)return false;
        try {pixels.resize(std::size_t(width)*height);}catch(const std::bad_alloc&){return false;}
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
            auto& p=pixels[std::size_t(y)*width+x];p.offset=UINT32_MAX;
            const float cx=2.f*(x+.5f)/width-1.f,cy=2.f*(y+.5f)/height-1.f;
            const float radial=1.f+.05f*(cx*cx+cy*cy);
            const float wx=cx*radial,wy=cy*radial;
            const float u=wx*.5f+.5f,v=wy*.5f+.5f;
            if(u<0.f || v<0.f || u>1.f || v>1.f)continue;
            const float sx=std::clamp(u*width-.5f,0.f,float(width-1));
            const float sy=std::clamp(v*height-.5f,0.f,float(height-1));
            const unsigned ix=linear?unsigned(sx):unsigned(sx+.5f),iy=linear?unsigned(sy):unsigned(sy+.5f);
            p.offset=std::uint32_t(iy*pitch+ix*bpp);
            p.fx=linear?std::uint8_t((sx-ix)*256.f):0;
            p.fy=linear?std::uint8_t((sy-iy)*256.f):0;
            p.edge=std::uint8_t((ix+1<width?1:0)|(iy+1<height?2:0));
            const float vignette=std::max(.58f,1.f-.22f*(wx*wx+wy*wy));
            const float scan=(y&1)? .68f:1.f;
            for(unsigned c=0;c<3;++c)p.gain[c]=std::uint16_t(256.f*vignette*scan*(x%3==c?1.f:.93f));
        }
        return ready_=true;
    }
    std::size_t bytes()const{return pixels.capacity()*sizeof(Pixel);}
    bool copy(void* destination,std::size_t stride,const void* source,unsigned width,unsigned height,
              std::size_t pitch,std::uint32_t format,bool convert,bool linear) {
        const unsigned bpp=format==RR_PIXEL_FORMAT_RGB565?2:format==RR_PIXEL_FORMAT_RGB888?3:
            (format==RR_PIXEL_FORMAT_XRGB8888 || format==RR_PIXEL_FORMAT_RGBA8888)?4:0;
        if(!destination || !source || !bpp || (convert && bpp!=2) || stride<std::size_t(width)*bpp ||
           (height && stride>(SIZE_MAX-std::size_t(width)*bpp)/height) || !prepare(width,height,pitch,bpp,linear))return false;
        auto* dst=static_cast<std::uint8_t*>(destination);const auto* src=static_cast<const std::uint8_t*>(source);
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
            const auto& p=pixels[std::size_t(y)*width+x];auto* out=dst+std::size_t(y)*stride+x*bpp;
            if(p.offset==UINT32_MAX){std::memset(out,0,bpp);if(bpp==4)out[3]=0xff;continue;}
            const auto* in=src+p.offset;
            auto read=[&](const std::uint8_t* at,unsigned c)->unsigned {
                if(bpp==2){auto value=go2_scanlines::load<std::uint16_t>(at);if(convert)value=go2_scanlines::convert1555(value);
                    return c==0?(value>>11)&31U:c==1?(value>>5)&63U:value&31U;}
                return at[bpp==4?2-c:c];
            };
            unsigned channels[3];
            for(unsigned c=0;c<3;++c){
                unsigned value=read(in,c);
                if(linear){const auto dx=(p.edge&1)?bpp:0U;const auto dy=(p.edge&2)?pitch:0;
                    const unsigned top=value*(256-p.fx)+read(in+dx,c)*p.fx;
                    const unsigned bottom=read(in+dy,c)*(256-p.fx)+read(in+dy+dx,c)*p.fx;
                    value=(top*(256-p.fy)+bottom*p.fy+32768U)>>16;}
                channels[c]=(value*p.gain[c]+128U)>>8;
            }
            if(bpp==2)go2_scanlines::store(out,std::uint16_t((channels[0]<<11)|(channels[1]<<5)|channels[2]));
            else {for(unsigned c=0;c<3;++c)out[bpp==4?2-c:c]=std::uint8_t(channels[c]);if(bpp==4)out[3]=in[3];}
        }
        return true;
    }
};
}
