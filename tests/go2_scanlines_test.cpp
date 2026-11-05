#include "video/go2_scanlines.h"
#include "fixtures/go2_scanlines_legacy.h"
#include <array>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <algorithm>
using Bytes=std::vector<std::uint8_t>;
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static unsigned randomValue(){static unsigned v=0x71be0321;v^=v<<13;v^=v>>17;v^=v<<5;return v;}
static void compare(unsigned w,unsigned h,unsigned bpp,unsigned fmt,bool convert,unsigned offset,unsigned extra){
    const auto srcStride=std::size_t(w)*bpp+extra,dstStride=std::size_t(w)*bpp+extra+3;
    Bytes input(offset+srcStride*h+32,0x6b),output(offset+dstStride*h+32,0xa5);
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w*bpp;++x)input[offset+y*srcStride+x]=randomValue();
    const auto saved=input;
    // The old 1555 copier silently skipped the last odd pixel. Give the golden
    // path a padded extra pixel so it defines the intended last-pixel conversion.
    const unsigned goldenWidth=w+unsigned(convert && (w&1));
    const auto goldenStride=(goldenWidth*bpp+15U)&~15U;
    Bytes aligned(goldenStride*h,0),expected(goldenStride*h,0xa5);
    for(unsigned y=0;y<h;++y)std::memcpy(aligned.data()+y*goldenStride,input.data()+offset+y*srcStride,w*bpp);
    legacy_scanlines::Surface target{expected.data(),int(goldenStride),fmt};legacy_scanlines::surface=&target;
    legacy_scanlines::color_format=convert?RR_PIXEL_FORMAT_RGBA5551:fmt;
    legacy_scanlines::draw(aligned.data(),goldenWidth,h,goldenStride);
    check(go2_scanlines::copy(output.data()+offset,dstStride,input.data()+offset,srcStride,w,h,fmt,convert),"valid upload rejected");
    check(input==saved,"core framebuffer modified");
    for(unsigned y=0;y<h;++y){
        check(!std::memcmp(output.data()+offset+y*dstStride,expected.data()+y*goldenStride,w*bpp),"pixels differ from pre-change scanlines");
        for(std::size_t x=w*bpp;x<dstStride;++x)check(output[offset+y*dstStride+x]==0xa5,"destination row padding overwritten");
    }
    for(unsigned i=0;i<offset;++i)check(output[i]==0xa5,"leading guard overwritten");
    for(std::size_t i=offset+dstStride*h;i<output.size();++i)check(output[i]==0xa5,"trailing guard overwritten");
}
int main(){try{
    for(unsigned p=0;p<65536;++p){
        const auto ref=[](unsigned v){const unsigned gain=static_cast<unsigned>(0.68f*256.f);return (v*gain+128U)>>8U;};
        const auto expected=(ref((p>>11)&31)<<11)|(ref((p>>5)&63)<<5)|ref(p&31);
        check(go2_scanlines::dim565(p)==expected,"exhaustive RGB565 mismatch");
        const unsigned converted=(((p<<1)&~0x3fU)|(p&31U))&65535;
        check(go2_scanlines::convert1555(p)==converted,"exhaustive 1555 conversion mismatch");
    }
    for(unsigned p=0;p<0x1000000U;++p){
        const unsigned alpha=(p*37U)&0xff000000U;
        const auto q=p|alpha;const auto ref=[](unsigned v){return (v*static_cast<unsigned>(0.68f*256.f)+128U)>>8U;};
        check(go2_scanlines::dim8888(q)==(alpha|(ref((q>>16)&255)<<16)|(ref((q>>8)&255)<<8)|ref(q&255)),"exhaustive XRGB8888 mismatch");
    }
    for(auto shape:{std::pair<unsigned,unsigned>{1,1},{1,5},{255,239},{256,240},{320,224},{640,480},{480,640},{720,576},{641,3}})
     for(unsigned offset:{0U,1U,3U})for(unsigned padding:{0U,2U,17U}){
      compare(shape.first,shape.second,2,RR_PIXEL_FORMAT_RGB565,false,offset,padding);
      compare(shape.first,shape.second,2,RR_PIXEL_FORMAT_RGB565,true,offset,padding);
      compare(shape.first,shape.second,4,RR_PIXEL_FORMAT_XRGB8888,false,offset,padding);
      compare(shape.first,shape.second,4,RR_PIXEL_FORMAT_RGBA8888,false,offset,padding);
      compare(shape.first,shape.second,3,RR_PIXEL_FORMAT_RGB888,false,offset,padding);
     }
    std::array<std::uint8_t,64> a{},b{};b.fill(0xc7);const auto original=b;
    check(!go2_scanlines::copy(nullptr,8,a.data(),8,2,2,RR_PIXEL_FORMAT_RGB565),"null accepted");
    check(!go2_scanlines::copy(b.data(),1,a.data(),8,2,2,RR_PIXEL_FORMAT_RGB565),"short stride accepted");
    check(!go2_scanlines::copy(b.data(),8,a.data(),8,2,2,0),"unknown format accepted");
    check(!go2_scanlines::copy(b.data(),8,a.data(),8,2,2,RR_PIXEL_FORMAT_XRGB8888,true),"invalid conversion accepted");
    check(!go2_scanlines::copy(b.data(),SIZE_MAX,a.data(),SIZE_MAX,2,4,RR_PIXEL_FORMAT_RGB565),"overflowing layout accepted");
    check(b==original,"invalid request wrote to destination");
    std::cout<<"Golden source-frame equivalence: all 65536 RGB565/1555 values, 16777216 RGB colors, odd/padded/unaligned layouts PASS\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
