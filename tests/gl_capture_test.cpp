#include "platform/platform.h"
#include <vector>
#include <cassert>
#include <cstring>
#include <iostream>
using GLint=int;using GLuint=unsigned;using GLenum=unsigned;
enum {GL_FRAMEBUFFER_BINDING=1,GL_PACK_ALIGNMENT,GL_READ_FRAMEBUFFER_BINDING,GL_PIXEL_PACK_BUFFER_BINDING,
GL_PIXEL_PACK_BUFFER,GL_PACK_ROW_LENGTH,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_PIXELS,GL_FRAMEBUFFER,
GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,GL_FRAMEBUFFER_COMPLETE,GL_RGBA,GL_UNSIGNED_BYTE,GL_DRAW_FRAMEBUFFER,GL_READ_FRAMEBUFFER};
static int draw=31,read=47,pack=8,pbo=71,row=90,rows=3,pixels=7,deleted=0,allocations=0;static bool complete=true,separate=true;
void glGetIntegerv(GLenum key,GLint* out){switch(key){case GL_FRAMEBUFFER_BINDING:*out=draw;break;case GL_READ_FRAMEBUFFER_BINDING:*out=read;break;case GL_PACK_ALIGNMENT:*out=pack;break;case GL_PIXEL_PACK_BUFFER_BINDING:*out=pbo;break;case GL_PACK_ROW_LENGTH:*out=row;break;case GL_PACK_SKIP_ROWS:*out=rows;break;case GL_PACK_SKIP_PIXELS:*out=pixels;break;default:assert(false);}}
void glBindBuffer(GLenum,GLuint value){pbo=value;}
void glPixelStorei(GLenum key,GLint value){switch(key){case GL_PACK_ALIGNMENT:pack=value;break;case GL_PACK_ROW_LENGTH:row=value;break;case GL_PACK_SKIP_ROWS:rows=value;break;case GL_PACK_SKIP_PIXELS:pixels=value;break;default:assert(false);}}
void glGenFramebuffers(int,GLuint* value){*value=7;}
void glBindFramebuffer(GLenum key,GLuint value){if(key==GL_FRAMEBUFFER)draw=read=value;else if(key==GL_DRAW_FRAMEBUFFER)draw=value;else if(key==GL_READ_FRAMEBUFFER)read=value;else assert(false);}
void glFramebufferTexture2D(GLenum,GLenum,GLenum,GLuint texture,int){assert(texture==9);}
GLenum glCheckFramebufferStatus(GLenum){return complete?GL_FRAMEBUFFER_COMPLETE:0;}
void glReadPixels(int,int,int w,int h,GLenum,GLenum,void* out){assert(draw==7 && read==7 && pack==1);if(separate)assert(pbo==0 && row==0 && rows==0 && pixels==0);auto* b=static_cast<unsigned char*>(out);for(int y=0;y<h;++y)for(int x=0;x<w;++x){auto* p=b+(y*w+x)*4;p[0]=10+y*10;p[1]=30+x;p[2]=50;p[3]=255;}}
void glDeleteFramebuffers(int,GLuint*){++deleted;}
struct rr_display{};struct rr_surface{int w,h;std::vector<unsigned char> bytes;};
rr_surface_t* rr_surface_create(rr_display_t*,int w,int h,uint32_t){++allocations;return new rr_surface{w,h,std::vector<unsigned char>(w*h*3)};}
void* rr_surface_map(rr_surface_t* s){return s->bytes.data();}
void rr_surface_unmap(rr_surface_t*){}
int rr_surface_stride_get(rr_surface_t* s){return s->w*3;}
void rr_surface_destroy(rr_surface_t* s){--allocations;delete s;}
#include "platform/gl_capture.h"
int main(){rr_display display;
 auto* image=captureGlTexture(&display,9,2,2,true);assert(image && image->bytes[0]==20 && image->bytes[1]==30 && image->bytes[6]==10);rr_surface_destroy(image);
 assert(draw==31 && read==47 && pack==8 && pbo==71 && row==90 && rows==3 && pixels==7 && deleted==1 && allocations==0);
 complete=false;assert(!captureGlTexture(&display,9,2,2,true));assert(draw==31 && read==47 && deleted==2 && allocations==0);
 complete=true;separate=false;read=draw=11;image=captureGlTexture(&display,9,2,2,false);assert(image);rr_surface_destroy(image);assert(draw==11 && read==11 && pack==8 && deleted==3 && allocations==0);
 assert(!captureGlTexture(&display,0,2,2,true) && !captureGlTexture(&display,9,8193,2,true));
 std::cout<<"GPU capture: RGB/orientation, GL pack/framebuffer state and cleanup passed\n";
}
