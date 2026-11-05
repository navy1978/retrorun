// Real EGL/GLES integration test: Mesa surfaceless or target GBM/EGL.
// Compile separately with function-section GC (see docs/go2-effects.md).
#include <GLES3/gl3.h>
#include "../src/platform/platform_go2.cpp"
#include <EGL/eglext.h>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <gbm.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
static bool testEs3=false;
static std::vector<unsigned char> presented;
static int imageWidth=64,imageHeight=48;
go2_context_t* go2_context_create(go2_display_t*,int,int,const go2_context_attributes_t*){return reinterpret_cast<go2_context_t*>(1);}
void go2_context_make_current(go2_context_t*){}
void go2_context_destroy(go2_context_t*){}
void go2_context_swap_buffers(go2_context_t*){
 // EGL presents the native window, independently of the core's current read
 // FBO. Model that in test-only readback and restore the binding afterwards.
 GLint previous=0;glGetIntegerv(testEs3?GL_READ_FRAMEBUFFER_BINDING:GL_FRAMEBUFFER_BINDING,&previous);
 glBindFramebuffer(testEs3?GL_READ_FRAMEBUFFER:GL_FRAMEBUFFER,0);
 presented.resize(imageWidth*imageHeight*4);glReadPixels(0,0,imageWidth,imageHeight,GL_RGBA,GL_UNSIGNED_BYTE,presented.data());
 glBindFramebuffer(testEs3?GL_READ_FRAMEBUFFER:GL_FRAMEBUFFER,previous);
}
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void white(GLuint framebuffer){glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glDisable(GL_SCISSOR_TEST);if(testEs3)glDisable(GL_RASTERIZER_DISCARD);
 glColorMask(1,1,1,1);glClearColor(1,1,1,1);glViewport(0,0,imageWidth,imageHeight);glClear(GL_COLOR_BUFFER_BIT);}
static void verify(rr_context_t* context,bool es3,int mode,bool newFrame=true){
 GLuint vao=0,vbo=0,sampler=0,readFbo=0,unpack=0;
 if(es3){glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenSamplers(1,&sampler);glBindSampler(0,sampler);glGenFramebuffers(1,&readFbo);glBindFramebuffer(GL_READ_FRAMEBUFFER,readFbo);
 glGenBuffers(1,&unpack);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,unpack);glBufferData(GL_PIXEL_UNPACK_BUFFER,8,nullptr,GL_STATIC_DRAW);}
 glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);float vertices[32]={};glBufferData(GL_ARRAY_BUFFER,sizeof vertices,vertices,GL_STATIC_DRAW);
 glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,20,(void*)4);glEnableVertexAttribArray(0);
 glEnable(GL_DEPTH_TEST);glDepthFunc(GL_NEVER);glEnable(GL_STENCIL_TEST);glStencilFunc(GL_NEVER,0,~0U);
 glEnable(GL_SCISSOR_TEST);glScissor(0,0,0,0);glEnable(GL_CULL_FACE);glCullFace(GL_FRONT_AND_BACK);
 glEnable(GL_BLEND);glBlendFunc(GL_ZERO,GL_ZERO);glColorMask(0,0,0,0);glClearColor(.1f,.2f,.3f,.4f);
 if(es3)glEnable(GL_RASTERIZER_DISCARD);
 glViewport(3,5,21,17);glActiveTexture(GL_TEXTURE3);
 GLint beforeFb=0;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&beforeFb);
 rr_context_swap_buffers(context,imageWidth,imageHeight,0,0,imageWidth,imageHeight,nullptr,RR_ROTATION_DEGREES_0,newFrame);
 const auto error=glGetError();if(error!=GL_NO_ERROR)std::cerr<<"post error: 0x"<<std::hex<<error<<std::dec<<"\n";check(error==GL_NO_ERROR,"GL error before pixel validation");
 auto red=[](int x,int y){return presented[(y*imageWidth+x)*4];};
 if(mode==1){check(red(30,20)>=172 && red(30,20)<=183,"scanline draw missing/blocked");check(red(30,21)==255,"bright row changed");}
 else if(mode==2){check(red(0,0)==0,"CRT corner not curved");check(red(31,25)>150,"CRT center missing");}
 else check(red(30,20)==255,"OFF passthrough changed");
 for(auto cap:{GL_DEPTH_TEST,GL_STENCIL_TEST,GL_SCISSOR_TEST,GL_CULL_FACE,GL_BLEND})check(glIsEnabled(cap),"core capability lost");
 GLboolean mask[4];glGetBooleanv(GL_COLOR_WRITEMASK,mask);check(!mask[0]&&!mask[1]&&!mask[2]&&!mask[3],"core color mask lost");
 GLint value,viewport[4];glGetIntegerv(GL_ACTIVE_TEXTURE,&value);check(value==GL_TEXTURE3,"active texture lost");
 glGetIntegerv(GL_FRAMEBUFFER_BINDING,&value);check(value==beforeFb,"core framebuffer lost");
 glGetIntegerv(GL_VIEWPORT,viewport);check(viewport[0]==3&&viewport[1]==5&&viewport[2]==21&&viewport[3]==17,"viewport lost");
 glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&value);check(value==int(vbo),"array buffer lost");
 glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_SIZE,&value);check(value==3,"core attribute damaged");
 void* pointer=nullptr;glGetVertexAttribPointerv(0,GL_VERTEX_ATTRIB_ARRAY_POINTER,&pointer);check(pointer==(void*)4,"core attribute pointer damaged");
 if(es3){glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&value);check(value==int(vao),"core VAO lost");
 glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&value);check(value==int(readFbo),"read framebuffer lost");
 glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&value);check(value==int(unpack),"unpack buffer lost");glActiveTexture(GL_TEXTURE0);
 glGetIntegerv(GL_SAMPLER_BINDING,&value);check(value==int(sampler),"core sampler lost");check(glIsEnabled(GL_RASTERIZER_DISCARD),"discard lost");
 glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glDeleteBuffers(1,&unpack);glBindFramebuffer(GL_READ_FRAMEBUFFER,0);glDeleteFramebuffers(1,&readFbo);
 glBindSampler(0,0);glDeleteSamplers(1,&sampler);glBindVertexArray(0);glDeleteVertexArrays(1,&vao);glDisable(GL_RASTERIZER_DISCARD);}
 glDeleteBuffers(1,&vbo);check(glGetError()==GL_NO_ERROR,"GL error in post processing");
}
int main(){try{
 auto platform=(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");check(platform,"EGL platform API missing");
 const char* drm=std::getenv("RETRORUN_TEST_DRM");int fd=-1;gbm_device* gbm=nullptr;
 if(drm){fd=open(drm,O_RDWR);check(fd>=0,"DRM open failed");gbm=gbm_create_device(fd);check(gbm,"GBM failed");}
 EGLDisplay display=platform(drm?EGL_PLATFORM_GBM_KHR:EGL_PLATFORM_SURFACELESS_MESA,drm?static_cast<void*>(gbm):nullptr,nullptr);
 if(!eglInitialize(display,nullptr,nullptr) && drm){display=eglGetDisplay((EGLNativeDisplayType)gbm);check(eglInitialize(display,nullptr,nullptr),"EGL GBM init failed");}
 check(display!=EGL_NO_DISPLAY,"EGL display failed");eglBindAPI(EGL_OPENGL_ES_API);
 for(bool rgb565:{true,false})for(int version:{2,3}){
 testEs3=version>=3;
 EGLint cfg[]={EGL_SURFACE_TYPE,drm?EGL_WINDOW_BIT:EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,version==3?EGL_OPENGL_ES3_BIT:EGL_OPENGL_ES2_BIT,EGL_RED_SIZE,rgb565?5:8,EGL_GREEN_SIZE,rgb565?6:8,EGL_BLUE_SIZE,rgb565?5:8,EGL_ALPHA_SIZE,rgb565?0:8,EGL_NONE};
 EGLConfig config=nullptr,choices[128];EGLint count;check(eglChooseConfig(display,cfg,choices,128,&count)&&count,"EGL config failed");
 for(int i=0;i<count;++i){EGLint r,g,b,a;eglGetConfigAttrib(display,choices[i],EGL_RED_SIZE,&r);eglGetConfigAttrib(display,choices[i],EGL_GREEN_SIZE,&g);eglGetConfigAttrib(display,choices[i],EGL_BLUE_SIZE,&b);eglGetConfigAttrib(display,choices[i],EGL_ALPHA_SIZE,&a);
 if(r==(rgb565?5:8)&&g==(rgb565?6:8)&&b==(rgb565?5:8)&&a==(rgb565?0:8)){config=choices[i];break;}}
 check(config,"exact RGB565/RGBA8888 config missing");
 EGLint size[]={EGL_WIDTH,64,EGL_HEIGHT,48,EGL_NONE},attrs[]={EGL_CONTEXT_CLIENT_VERSION,version,EGL_NONE};
 gbm_surface* window=nullptr;
 if(drm){EGLint visual=0;eglGetConfigAttrib(display,config,EGL_NATIVE_VISUAL_ID,&visual);window=gbm_surface_create(gbm,64,48,visual,GBM_BO_USE_RENDERING|GBM_BO_USE_SCANOUT);check(window,"GBM surface failed");}
 auto surface=drm?eglCreateWindowSurface(display,config,(EGLNativeWindowType)window,nullptr):eglCreatePbufferSurface(display,config,size);auto glcontext=eglCreateContext(display,config,EGL_NO_CONTEXT,attrs);check(eglMakeCurrent(display,surface,surface,glcontext),"EGL context failed");
 rr_display_t fake{};rr_context_attributes_t request{};request.major=version;request.red_bits=rgb565?5:8;request.green_bits=rgb565?6:8;request.blue_bits=rgb565?5:8;request.alpha_bits=rgb565?0:8;
 rr_video_shader_set(RR_VIDEO_SHADER_OFF);auto* context=rr_context_create(&fake,64,48,&request);check(context,"context failed");
 // OFF->ON after a default-buffer frame has already finished.
 white(0);rr_video_shader_set(RR_VIDEO_SHADER_SCANLINES);verify(context,version>=3,1);
 // Duplicate callback cannot capture/re-darken the already processed frame.
 verify(context,version>=3,1,false);
 // A core caching its original framebuffer 0 remains supported.
 white(0);verify(context,version>=3,1);
 // Framebuffer queries remain stable, including OFF->ON transitions.
 check(rr_context_framebuffer_get(context)==0,"shader toggle changed the core target");
 white(rr_context_framebuffer_get(context));verify(context,version>=3,1);
 white(rr_context_framebuffer_get(context));GLuint privateTarget;glGenFramebuffers(1,&privateTarget);glBindFramebuffer(GL_FRAMEBUFFER,privateTarget);verify(context,version>=3,1);
 glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteFramebuffers(1,&privateTarget);
 white(rr_context_framebuffer_get(context));rr_video_shader_set(RR_VIDEO_SHADER_CRT);verify(context,version>=3,2);
 white(rr_context_framebuffer_get(context));rr_video_shader_set(RR_VIDEO_SHADER_OFF);verify(context,version>=3,0);
 rr_context_destroy(context);eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(display,glcontext);eglDestroySurface(display,surface);if(window)gbm_surface_destroy(window);
 }
 eglTerminate(display);if(gbm)gbm_device_destroy(gbm);if(fd>=0)close(fd);std::cout<<"RGB565/RGBA8888 GLES2/3 OFF->ON, stable default/private bindings, scanlines/CRT/OFF, hostile draw-state preservation PASS\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
