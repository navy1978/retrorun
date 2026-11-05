#pragma once
#include <GLES3/gl3.h>

// GO2 shares its context with the libretro core. The post pass must neither
// inherit the core's draw tests/VAO/sampler nor change its cached GL state.
class Go2GlState {
    struct Attribute {GLint enabled,size,type,normalized,stride,buffer;void* pointer;};
    static constexpr GLenum caps[]={GL_BLEND,GL_DEPTH_TEST,GL_STENCIL_TEST,GL_SCISSOR_TEST,
        GL_CULL_FACE,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE,GL_DITHER};
    bool es3;
    GLboolean enabled[8],color[4],discard=GL_FALSE;
    GLfloat clear[4];
    GLint program,array,active,texture,renderbuffer,viewport[4],read=0,vao=0,sampler=0,unpack=0;
    Attribute attributes[2]{};
public:
    GLint framebuffer=0;
    Go2GlState(const Go2GlState&)=delete;
    Go2GlState& operator=(const Go2GlState&)=delete;
    explicit Go2GlState(bool version3):es3(version3) {
        glGetIntegerv(GL_FRAMEBUFFER_BINDING,&framebuffer);
        glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&array);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);glGetIntegerv(GL_RENDERBUFFER_BINDING,&renderbuffer);
        glGetIntegerv(GL_VIEWPORT,viewport);glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);glGetBooleanv(GL_COLOR_WRITEMASK,color);
        for(unsigned i=0;i<8;++i)enabled[i]=glIsEnabled(caps[i]);
        if(es3){glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
            glGetIntegerv(GL_SAMPLER_BINDING,&sampler);glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpack);
            discard=glIsEnabled(GL_RASTERIZER_DISCARD);}
        else for(unsigned i=0;i<2;++i){auto& a=attributes[i];
            glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&a.enabled);
            glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_SIZE,&a.size);
            glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_TYPE,&a.type);
            glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,&a.normalized);
            glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_STRIDE,&a.stride);
            glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&a.buffer);
            glGetVertexAttribPointerv(i,GL_VERTEX_ATTRIB_ARRAY_POINTER,&a.pointer);}
    }
    void prepare(GLuint postVao=0) {
        for(auto cap:caps)glDisable(cap);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        if(es3){glDisable(GL_RASTERIZER_DISCARD);glBindVertexArray(postVao);
            glBindSampler(0,0);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);}
    }
    ~Go2GlState(){
        if(es3){glBindVertexArray(vao);glBindSampler(0,sampler);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,unpack);
            if(discard)glEnable(GL_RASTERIZER_DISCARD);else glDisable(GL_RASTERIZER_DISCARD);}
        else for(unsigned i=0;i<2;++i){const auto& a=attributes[i];glBindBuffer(GL_ARRAY_BUFFER,a.buffer);
            glVertexAttribPointer(i,a.size,a.type,a.normalized,a.stride,a.pointer);
            if(a.enabled)glEnableVertexAttribArray(i);else glDisableVertexAttribArray(i);}
        for(unsigned i=0;i<8;++i){if(enabled[i])glEnable(caps[i]);else glDisable(caps[i]);}
        glColorMask(color[0],color[1],color[2],color[3]);glClearColor(clear[0],clear[1],clear[2],clear[3]);
        glUseProgram(program);glBindBuffer(GL_ARRAY_BUFFER,array);glBindRenderbuffer(GL_RENDERBUFFER,renderbuffer);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);glActiveTexture(active);
        glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);if(es3)glBindFramebuffer(GL_READ_FRAMEBUFFER,read);
    }
};
