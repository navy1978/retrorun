// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform.h"
#include <algorithm>
#include <vector>

// Read an existing raw GPU snapshot only when a capture is requested. Preserve
// both framebuffer bindings and pack state; never change the core's FBO target.
inline rr_surface_t* captureGlTexture(rr_display_t* display,GLuint texture,int width,int height,bool separateBindings){
    if(!texture || width<=0 || height<=0 || width>8192 || height>8192)return nullptr;
    GLint draw=0,read=0,pack=0,pbo=0,rowLength=0,skipRows=0,skipPixels=0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_PACK_ALIGNMENT,&pack);
    if(separateBindings){
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&pbo);glBindBuffer(GL_PIXEL_PACK_BUFFER,0);
        glGetIntegerv(GL_PACK_ROW_LENGTH,&rowLength);glGetIntegerv(GL_PACK_SKIP_ROWS,&skipRows);glGetIntegerv(GL_PACK_SKIP_PIXELS,&skipPixels);
        glPixelStorei(GL_PACK_ROW_LENGTH,0);glPixelStorei(GL_PACK_SKIP_ROWS,0);glPixelStorei(GL_PACK_SKIP_PIXELS,0);
    }
    GLuint fbo=0;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
    const bool complete=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    std::vector<unsigned char> rgba;
    if(complete){rgba.resize(std::size_t(width)*height*4);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());}
    glPixelStorei(GL_PACK_ALIGNMENT,pack);
    if(separateBindings){
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);glBindFramebuffer(GL_READ_FRAMEBUFFER,read);
        glBindBuffer(GL_PIXEL_PACK_BUFFER,pbo);glPixelStorei(GL_PACK_ROW_LENGTH,rowLength);glPixelStorei(GL_PACK_SKIP_ROWS,skipRows);glPixelStorei(GL_PACK_SKIP_PIXELS,skipPixels);
    }else glBindFramebuffer(GL_FRAMEBUFFER,draw);
    glDeleteFramebuffers(1,&fbo);
    if(!complete)return nullptr;
    auto* image=rr_surface_create(display,width,height,RR_PIXEL_FORMAT_RGB888);if(!image)return nullptr;
    auto* dst=static_cast<unsigned char*>(rr_surface_map(image));if(!dst){rr_surface_destroy(image);return nullptr;}
    const int stride=rr_surface_stride_get(image);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        const auto* pixel=rgba.data()+(std::size_t(height-1-y)*width+x)*4;
        auto* out=dst+std::size_t(y)*stride+x*3;out[0]=pixel[0];out[1]=pixel[1];out[2]=pixel[2];
    }
    rr_surface_unmap(image);return image;
}
