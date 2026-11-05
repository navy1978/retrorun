#pragma once
#include "go2_scanlines.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <new>
#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace go2_crt {
// CPU inverse warp in source-frame coordinates. Same radial coefficient as the
// GO2 GLES CRT shader; RGA still owns final viewport/scaling/rotation.
class Plan {
    struct Pixel {std::uint32_t offset;std::uint16_t gain[3];std::uint8_t fx,fy,edge;};
    std::vector<Pixel> pixels;
    unsigned width_=0,height_=0,bpp_=0;
    std::size_t pitch_=0;
    bool linear_=false,ready_=false;
    template<unsigned Bpp,bool Convert> static std::uint32_t read(const std::uint8_t* at){
        if constexpr(Bpp==2){auto value=go2_scanlines::load<std::uint16_t>(at);if constexpr(Convert)value=go2_scanlines::convert1555(value);return value;}
        else if constexpr(Bpp==4)return go2_scanlines::load<std::uint32_t>(at);
        else return (unsigned(at[0])<<16)|(unsigned(at[1])<<8)|at[2];
    }
    template<unsigned Bpp,bool Convert,bool Linear> void render(void* destination,std::size_t stride,const void* source)const{
        constexpr unsigned redShift=Bpp==2?11:16,greenShift=Bpp==2?5:8,rbMask=Bpp==2?31:255,gMask=Bpp==2?63:255;
        const auto* src=static_cast<const std::uint8_t*>(source);auto* dst=static_cast<std::uint8_t*>(destination);
        for(unsigned y=0;y<height_;++y)for(unsigned x=0;x<width_;++x){
            const auto& p=pixels[std::size_t(y)*width_+x];auto* out=dst+std::size_t(y)*stride+x*Bpp;
            if(p.offset==UINT32_MAX){std::memset(out,0,Bpp);if constexpr(Bpp==4)out[3]=0xff;continue;}
            const auto* in=src+p.offset;const auto pixel=read<Bpp,Convert>(in);
            unsigned r=(pixel>>redShift)&rbMask,g=(pixel>>greenShift)&gMask,b=pixel&rbMask;
            if constexpr(Linear){
                const auto dx=(p.edge&1)?Bpp:0U;const auto dy=(p.edge&2)?pitch_:0;
                const auto p10=read<Bpp,Convert>(in+dx),p01=read<Bpp,Convert>(in+dy),p11=read<Bpp,Convert>(in+dy+dx);
#if defined(__ARM_NEON)
                if constexpr(Bpp==4){
                    // Measured hot path: four 8-bit channels in parallel, with
                    // full Q16 bilinear precision and exact scalar rounding.
                    auto unpack=[](std::uint32_t v){return vget_low_u16(vmovl_u8(vcreate_u8(v)));};
                    const auto top=vmla_n_u16(vmul_n_u16(unpack(pixel),256-p.fx),unpack(p10),p.fx);
                    const auto bottom=vmla_n_u16(vmul_n_u16(unpack(p01),256-p.fx),unpack(p11),p.fx);
                    const auto sum=vmlal_n_u16(vmull_n_u16(top,256-p.fy),bottom,p.fy);
                    const auto sample=vshrn_n_u32(vaddq_u32(sum,vdupq_n_u32(32768)),16);
                    auto gain=vdup_n_u16(256);gain=vset_lane_u16(p.gain[2],gain,0);
                    gain=vset_lane_u16(p.gain[1],gain,1);gain=vset_lane_u16(p.gain[0],gain,2);
                    const auto shaded=vshrn_n_u32(vaddq_u32(vmull_u16(sample,gain),vdupq_n_u32(128)),8);
                    const auto packed=vmovn_u16(vcombine_u16(shaded,vdup_n_u16(0)));
                    const auto value=vget_lane_u32(vreinterpret_u32_u8(packed),0);
                    go2_scanlines::store(out,(value&0x00ffffffU)|(pixel&0xff000000U));
                    continue;
                }
#endif
                // Independent 32-bit R/B lanes retain full Q16 precision.
                // Each weighted sum + rounding is <2^24, so there is no carry
                // between lanes. This is exactly the scalar 4-sample result.
                auto rb=[](std::uint32_t v){return (std::uint64_t((v>>redShift)&rbMask)<<32)|(v&rbMask);};
                const auto top=rb(pixel)*(256-p.fx)+rb(p10)*p.fx;
                const auto bottom=rb(p01)*(256-p.fx)+rb(p11)*p.fx;
                const auto sum=top*(256-p.fy)+bottom*p.fy+0x0000800000008000ULL;
                r=unsigned(sum>>48)&rbMask;b=unsigned(sum>>16)&rbMask;
                const unsigned gt=g*(256-p.fx)+((p10>>greenShift)&gMask)*p.fx;
                const unsigned gb=((p01>>greenShift)&gMask)*(256-p.fx)+((p11>>greenShift)&gMask)*p.fx;
                g=(gt*(256-p.fy)+gb*p.fy+32768U)>>16;
            }
            r=(r*p.gain[0]+128U)>>8;g=(g*p.gain[1]+128U)>>8;b=(b*p.gain[2]+128U)>>8;
            if constexpr(Bpp==2)go2_scanlines::store(out,std::uint16_t((r<<11)|(g<<5)|b));
            else if constexpr(Bpp==4)go2_scanlines::store(out,(pixel&0xff000000U)|(r<<16)|(g<<8)|b);
            else {out[0]=std::uint8_t(r);out[1]=std::uint8_t(g);out[2]=std::uint8_t(b);}
        }
    }

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
        if(bpp==2){
            if(convert){if(linear)render<2,true,true>(destination,stride,source);else render<2,true,false>(destination,stride,source);}
            else {if(linear)render<2,false,true>(destination,stride,source);else render<2,false,false>(destination,stride,source);}
        }else if(bpp==3){if(linear)render<3,false,true>(destination,stride,source);else render<3,false,false>(destination,stride,source);}
        else {if(linear)render<4,false,true>(destination,stride,source);else render<4,false,false>(destination,stride,source);}

        return true;
    }
};
}
