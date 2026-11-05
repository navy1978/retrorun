#include "video/go2_crt.h"
#include "fixtures/go2_crt_scalar.h"
#include <vector>
#include <iostream>
#include <stdexcept>
#include <fstream>
static void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
 for(auto format:{RR_PIXEL_FORMAT_RGB565,RR_PIXEL_FORMAT_RGB888,RR_PIXEL_FORMAT_XRGB8888,RR_PIXEL_FORMAT_RGBA8888})
 for(bool linear:{false,true})for(bool convert:{false,true}){
  const unsigned bpp=format==RR_PIXEL_FORMAT_RGB565?2:format==RR_PIXEL_FORMAT_RGB888?3:4;
  if(convert && bpp!=2)continue;
  for(auto shape:{std::pair<unsigned,unsigned>{1,1},{1,7},{9,1},{63,47},{256,240},{641,3}}){
   auto [w,h]=shape;const unsigned stride=w*bpp+7,pitch=w*bpp+3;
   std::vector<unsigned char> input(1+pitch*h+32,255),output(3+stride*h+32,0xa5);
   for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w*bpp;++x)input[1+y*pitch+x]=std::uint8_t((x*37+y*59+(x*y)%193)&255);
   const auto saved=input;go2_crt::Plan plan;
   check(plan.copy(output.data()+3,stride,input.data()+1,w,h,pitch,format,convert,linear),"CRT copy failed");
   auto expected=std::vector<unsigned char>(output.size(),0xa5);go2_crt_scalar::Plan reference;
   check(reference.copy(expected.data()+3,stride,input.data()+1,w,h,pitch,format,convert,linear),"scalar reference failed");
   check(output==expected,"packed CRT differs from scalar reference");
   check(input==saved,"core input changed");check(plan.bytes()<=16*1024*1024,"map budget exceeded");
   const auto previous=output;check(plan.copy(output.data()+3,stride,input.data()+1,w,h,pitch,format,convert,linear),"cached plan failed");check(output==previous,"unstable CRT output");
   for(unsigned y=0;y<h;++y)for(unsigned x=w*bpp;x<stride;++x)check(output[3+y*stride+x]==0xa5,"padding overwritten");
   for(unsigned x=0;x<3;++x)check(output[x]==0xa5,"leading guard overwritten");
   for(unsigned x=3+stride*h;x<output.size();++x)check(output[x]==0xa5,"trailing guard overwritten");
   if(w>32 && h>32){for(unsigned c=0;c<3 && c<bpp;++c)check(output[3+c]==0,"missing curved black corner");
 }
  }
 }
 unsigned char in[64]={},out[64]={};go2_crt::Plan plan;
 check(!plan.copy(out,8,in,2,2,8,0,false,false),"unknown format accepted");
 check(!plan.copy(out,1,in,2,2,8,RR_PIXEL_FORMAT_RGB565,false,false),"short destination pitch accepted");
 check(!plan.prepare(4096,4096,16384,4,true),"oversized map accepted");
 check(!plan.prepare(10,10,SIZE_MAX,4,true),"overflowing source accepted");
 if(argc>1){const unsigned w=256,h=240;std::vector<unsigned char> image(w*h*3),result(w*h*3);
  for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){auto i=(y*w+x)*3;image[i]=x%32<2||y%24<2?255:40;image[i+1]=image[i];image[i+2]=image[i];}
  check(plan.copy(result.data(),w*3,image.data(),w,h,w*3,RR_PIXEL_FORMAT_RGB888,false,true),"preview failed");
  std::ofstream file(argv[1],std::ios::binary);file<<"P6\n256 240\n255\n";file.write(reinterpret_cast<const char*>(result.data()),result.size());
 }
 std::cout<<"CRT formats, curvature, cache reuse, guards, odd/padded/unaligned layouts and bounds PASS\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
