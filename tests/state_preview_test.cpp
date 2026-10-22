#include "state_file.h"
#include "screenshot_capture.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#include <iostream>
#include <stdexcept>

struct rr_display{};
struct rr_surface{int width,height;std::vector<unsigned char> bytes;};
static int allocations=0;static bool failWrite=false;static rr_surface saved{};
rr_surface_t* rr_surface_create(rr_display_t*,int w,int h,uint32_t){++allocations;return new rr_surface{w,h,std::vector<unsigned char>(w*h*3)};}
void rr_surface_destroy(rr_surface_t* surface){--allocations;delete surface;}
int rr_surface_stride_get(rr_surface_t* surface){return surface->width*3;}
void* rr_surface_map(rr_surface_t* surface){return surface->bytes.data();}
void rr_surface_unmap(rr_surface_t*){}
void rr_surface_blit_pixels(rr_surface_t* src,int sx,int sy,int sw,int sh,rr_surface_t* dst,int,int,int dw,int dh){
    for(int y=0;y<dh;++y)for(int x=0;x<dw;++x)for(int c=0;c<3;++c)
        dst->bytes[(y*dw+x)*3+c]=src->bytes[((sy+y*sh/dh)*src->width+sx+x*sw/dw)*3+c];
}
int rr_surface_save_as_png(rr_surface_t* image,const char*){saved=*image;return failWrite?-1:0;}
static std::string read(const std::filesystem::path& p){std::ifstream input(p,std::ios::binary);return {std::istreambuf_iterator<char>(input),{}};}
int main(){
    char pattern[]="/tmp/retrorun-state-preview-XXXXXX";const char* temp=mkdtemp(pattern);assert(temp);
    const std::filesystem::path root=temp;
    struct Cleanup{std::filesystem::path p;~Cleanup(){std::filesystem::remove_all(p);}} cleanup{root};
    const auto state=root/"Game.rrstate.auto";const auto image=state_file::previewPath(state.string());
    const std::string first="original-state",second="new-state";
    assert(state_file::write(state,first.data(),first.size()));
    assert(state_file::preview(state,[](const char* path){std::ofstream(path)<<"PNG-fixture";return true;}));
    assert(read(image)=="PNG-fixture");
    assert(!state_file::write(state,nullptr,0));assert(read(state)==first && read(image)=="PNG-fixture");
    assert(state_file::write(state,second.data(),second.size()));assert(!std::filesystem::exists(image));
    assert(!state_file::preview(state,[](const char* path){std::ofstream(path)<<"partial";return false;}));
    assert(!std::filesystem::exists(image) && read(state)==second);
    assert(!state_file::preview(state,[](const char*)->bool{throw std::runtime_error("capture failed");}));
    assert(!state_file::preview(state,[](const char*){return true;})); // Empty output rejected.
    const auto other=root/"Game.rrstate.auto1";assert(state_file::write(other,first.data(),first.size()));
    assert(state_file::preview(other,[](const char* path){std::ofstream(path)<<"slot2-preview";return true;}));
    assert(state_file::preview(state,[](const char* path){std::ofstream(path)<<"slot1-preview";return true;}));
    assert(read(image)=="slot1-preview" && read(state_file::previewPath(other))=="slot2-preview");
    const auto longState=root/std::string(250,'x');assert(state_file::write(longState,first.data(),first.size()));
    assert(state_file::preview(longState,[](const char* path){std::ofstream(path)<<"long-name-preview";return true;}));
    const auto longest=root/std::string(253,'y');assert(state_file::write(longest,first.data(),first.size()));
    assert(!state_file::preview(longest,[](const char* path){std::ofstream(path)<<"valid-state-no-preview";return true;}));
    for(const auto& file:std::filesystem::directory_iterator(root))assert(file.path().filename().string().find(".rrstate-tmp-")!=0);
    rr_display display;rr_surface source{640,480,std::vector<unsigned char>(640*480*3,120)};
    assert(captureSurfacePng(&display,&source,0,0,640,480,"fixture",320,240,RR_ROTATION_DEGREES_0));assert(saved.width==320 && saved.height==240 && allocations==0);
    source={240,480,std::vector<unsigned char>(240*480*3)};
    assert(captureSurfacePng(&display,&source,0,0,240,480,"fixture",320,240,RR_ROTATION_DEGREES_0));assert(saved.width==120 && saved.height==240);
    source={2,3,std::vector<unsigned char>(18)};for(int i=0;i<6;++i)source.bytes[i*3]=static_cast<unsigned char>(i+1);
    assert(captureSurfacePng(&display,&source,0,0,2,3,"fixture",320,240,RR_ROTATION_DEGREES_90));assert(saved.width==3 && saved.height==2 && saved.bytes[0]==5 && saved.bytes[6]==1);
    assert(captureSurfacePng(&display,&source,0,0,2,3,"fixture",320,240,RR_ROTATION_DEGREES_270));assert(saved.bytes[0]==2 && saved.bytes[6]==6);
    assert(captureSurfacePng(&display,&source,0,1,2,2,"fixture",320,240,RR_ROTATION_DEGREES_0));assert(saved.bytes[0]==3);
    failWrite=true;assert(!captureSurfacePng(&display,&source,0,0,2,3,"fixture",320,240,RR_ROTATION_DEGREES_0));assert(allocations==0);
    assert(!captureSurfacePng(&display,nullptr,0,0,2,3,"fixture",320,240,RR_ROTATION_DEGREES_0));
    std::cout<<"Atomic state/preview, slot isolation, failed capture, bounds, aspect, rotation and cleanup passed\n";
}
