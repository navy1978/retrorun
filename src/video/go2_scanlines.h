#pragma once
// GO2 software upload only. The historical effect is in CORE-frame coordinates,
// before RGA scaling/viewport/rotation: even source rows unchanged, odd rows Q8
// gain floor(0.68f * 256) == 174, with +128 rounding per native component.
// Never read the destination DRM mapping; it may be expensive CPU-read memory.
#include "platform/platform.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace go2_scanlines {
inline unsigned dim(unsigned channel) { return (channel * 174U + 128U) >> 8U; }
inline std::uint16_t dim565(std::uint16_t p) {
    return static_cast<std::uint16_t>((dim((p >> 11) & 31U) << 11) |
                                     (dim((p >> 5) & 63U) << 5) | dim(p & 31U));
}
inline std::uint32_t dim8888(std::uint32_t p) {
    // Two independent 16-bit R/B lanes. Each product plus rounding is <65536,
    // so no carry can cross lanes. Preserve the same high byte as the old pass.
    const auto rb = ((((p & 0x00ff00ffU) * 174U + 0x00800080U) >> 8U) & 0x00ff00ffU);
    const auto g = ((((p & 0x0000ff00U) * 174U + 0x00008000U) >> 8U) & 0x0000ff00U);
    return (p & 0xff000000U) | rb | g;
}
inline std::uint16_t convert1555(std::uint16_t p) {
    // Preserve the original 0RGB1555->RGB565 mapping, including its zero green
    // LSB; do not substitute bit replication or a different color conversion.
    return static_cast<std::uint16_t>(((unsigned(p) << 1U) & ~0x3fU) | (p & 31U));
}
template<class T> inline T load(const std::uint8_t* p) { T value;std::memcpy(&value,p,sizeof value);return value; }
template<class T> inline void store(std::uint8_t* p,T value) { std::memcpy(p,&value,sizeof value); }
template<bool Convert> inline void row565(std::uint8_t* dst,const std::uint8_t* src,unsigned width,bool dark) {
    if(!dark && !Convert){if(dst!=src)std::memcpy(dst,src,std::size_t(width)*2);return;}
    for(unsigned x=0;x<width;++x){
        auto p=load<std::uint16_t>(src+std::size_t(x)*2);
        if constexpr(Convert)p=convert1555(p);
        if(dark)p=dim565(p);
        store(dst+std::size_t(x)*2,p);
    }
}
inline bool copy(void* destination,std::size_t dstPitch,const void* source,std::size_t srcPitch,
                 unsigned width,unsigned height,std::uint32_t format,bool convert=false) {
    unsigned bpp=0;
    switch(format){
    case RR_PIXEL_FORMAT_RGB565:bpp=2;break;
    case RR_PIXEL_FORMAT_XRGB8888:case RR_PIXEL_FORMAT_RGBA8888:bpp=4;break;
    case RR_PIXEL_FORMAT_RGB888:bpp=3;break;
    default:return false;
    }
    if(convert && format!=RR_PIXEL_FORMAT_RGB565)return false;
    if(!destination || !source || !width || !height)return false;
    if(width>std::numeric_limits<std::size_t>::max()/bpp)return false;
    const auto bytes=std::size_t(width)*bpp;
    if(dstPitch<bytes || srcPitch<bytes)return false;
    const auto rows=std::size_t(height-1);
    if(rows && (dstPitch>(std::numeric_limits<std::size_t>::max()-bytes)/rows ||
                srcPitch>(std::numeric_limits<std::size_t>::max()-bytes)/rows))return false;
    auto* dst=static_cast<std::uint8_t*>(destination);auto* src=static_cast<const std::uint8_t*>(source);
    for(unsigned y=0;y<height;++y){
        const bool dark=(y&1U)!=0;
        if(bpp==2){if(convert)row565<true>(dst,src,width,dark);else row565<false>(dst,src,width,dark);}
        else if(!dark){if(dst!=src)std::memcpy(dst,src,bytes);}
        else if(bpp==4){for(unsigned x=0;x<width;++x)store(dst+std::size_t(x)*4,dim8888(load<std::uint32_t>(src+std::size_t(x)*4)));}
        else {for(std::size_t i=0;i<bytes;++i)dst[i]=static_cast<std::uint8_t>(dim(src[i]));}
        if(y+1<height){dst+=dstPitch;src+=srcPitch;}
    }
    return true;
}
}
