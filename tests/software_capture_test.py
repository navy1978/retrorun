"""Compile the actual software upload function and verify effects can be bypassed."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'src/video/video.cpp').read_text()
body=source[source.index('bool drawNonOpenGL('):source.index('\nint timeCorrectFrame')]
harness=r'''
#define RR_PLATFORM_SDL 1
#include "platform/platform.h"
#include <vector>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <cstdint>
struct rr_surface {int w,h;std::vector<uint16_t> bytes;};
static rr_surface screen{4,4,std::vector<uint16_t>(16)},clean=screen;
static rr_surface* surface=&screen;
static auto videoShader=RR_VIDEO_SHADER_SCANLINES;
static uint32_t color_format=RR_PIXEL_FORMAT_RGB565;
void* rr_surface_map(rr_surface_t* s){return s->bytes.data();}
void rr_surface_unmap(rr_surface_t*){}
uint32_t rr_surface_format_get(rr_surface_t*){return RR_PIXEL_FORMAT_RGB565;}
int rr_surface_stride_get(rr_surface_t* s){return s->w*2;}
int rr_pixel_format_bpp(uint32_t){return 16;}
'''+body+r'''
int main(){uint16_t raw[16];std::fill(raw,raw+16,0xffff);
 assert(drawNonOpenGL(raw,4,4,8,nullptr,true));assert(screen.bytes[0]==0xffff && screen.bytes[4]<0xffff);
 auto before=screen.bytes;assert(drawNonOpenGL(raw,4,4,8,&clean,false));for(auto p:clean.bytes)assert(p==0xffff);assert(screen.bytes==before);
 color_format=RR_PIXEL_FORMAT_RGBA5551;std::fill(raw,raw+16,0x7fff);
 assert(drawNonOpenGL(raw,4,4,8,&clean,false));for(auto p:clean.bytes)assert(p==0xffdf);
}
'''
with tempfile.TemporaryDirectory(prefix='rr-clean-preview-') as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(harness)
 subprocess.run(['c++','-std=c++20','-Wall','-Wextra','-Werror','-I'+str(root/'src'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Actual software upload: effect screenshot, clean preview, 1555 conversion and live frame preservation PASS')
