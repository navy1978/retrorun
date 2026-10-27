// Off-screen render fixture: real OSD renderers, no emulator or display needed.
#include "globals.h"
#include "video-helper.h"
#include "ui-renderer.h"
#include "fonts.h"
#include "input.h"
#include "status.h"
#include "osd-theme.h"
#include "keyboard.h"
#ifdef RR_OSD_SDL_PROBE
// Test the real SDL composition path against an off-screen software framebuffer.
#include "../src/platform/platform_sdl.cpp"
#endif
#include <fstream>
#include <string>
#include <filesystem>
#include <cassert>
static int enabled() { return 1; }
static void capture(rr_surface_t* s, const std::string& path, bool cropFooter=false) {
    assert(s);
    auto* p=static_cast<uint16_t*>(rr_surface_map(s));
    std::ofstream out(path,std::ios::binary);
    const int width=rr_surface_width_get(s), height=rr_surface_height_get(s)-(cropFooter?12:0);
    out<<"P6\n"<<width<<" "<<height<<"\n255\n";
    const int stride=rr_surface_stride_get(s)/2;
    for(int y=0;y<height;++y) for(int x=0;x<width;++x) {
        auto c=p[y*stride+x];const unsigned char rgb[]={static_cast<unsigned char>(((c>>11)&31)*255/31),static_cast<unsigned char>(((c>>5)&63)*255/63),static_cast<unsigned char>((c&31)*255/31)};
        out.write(reinterpret_cast<const char*>(rgb),3);
    }
    rr_surface_unmap(s);
}
int main(int argc,char** argv) {
    assert(argc==2 || argc==3);
    if(argc==3) osd::select(argv[2]);std::filesystem::create_directories(argv[1]);
    setUIProfile(UIProfile::Handheld);updateUIMenuDimensions(640,480);
    Menu menu("Video",{MenuItem("Integer Scale",enabled,[](int){},"bool"),MenuItem("Aspect Ratio",enabled,[](int){},"")});
    menu.resetSelected(0);menuManager.setCurrentMenu(&menu);
    auto* surface=rr_surface_create(nullptr,INFO_MENU_WIDTH,INFO_MENU_HEIGHT,format_565);
    drawMenuInfoBackgroud(surface,INFO_MENU_WIDTH,INFO_MENU_HEIGHT);showInfo(640,&surface);
    capture(surface,std::string(argv[1])+"/menu.ppm",true);
#ifdef RR_OSD_SDL_PROBE
    auto* screen=rr_surface_create(nullptr,640,480,format_565);
    auto* game=rr_surface_create(nullptr,320,240,format_565);
    auto* gamePixels=static_cast<uint16_t*>(rr_surface_map(game));
    for(int y=0;y<240;++y)for(int x=0;x<320;++x)
        gamePixels[y*320+x]=(x/32+y/32)%2 ? osd::rgb565(40,80,200) : osd::rgb565(20,120,180);
    rr_surface_unmap(game);
    SDL_Surface* target=SDL_CreateRGBSurfaceWithFormatFrom(screen->pixels.data(),640,480,16,screen->stride,SDL_PIXELFORMAT_RGB565);
    assert(target);rr_display_t output{};output.width=640;output.height=480;
    rr_presenter_t presenter{};presenter.display=&output;presenter.renderer=SDL_CreateSoftwareRenderer(target);
    assert(presenter.renderer);SDL_Rect destination{0,0,640,480};
    render_surface(&presenter,game,nullptr,&destination,RR_ROTATION_DEGREES_0);
    render_surface(&presenter,surface,nullptr,&destination,RR_ROTATION_DEGREES_0,osd::theme().menuOpacity);
    SDL_RenderPresent(presenter.renderer);
    Uint8 alpha=0;SDL_BlendMode blend=SDL_BLENDMODE_INVALID;
    SDL_GetTextureAlphaMod(presenter.textures[surface].texture,&alpha);
    SDL_GetTextureBlendMode(presenter.textures[surface].texture,&blend);
    assert(alpha==255 && blend==SDL_BLENDMODE_NONE);
    capture(screen,std::string(argv[1])+"/menu-640x480.ppm");
    for(auto& item:presenter.textures)SDL_DestroyTexture(item.second.texture);
    SDL_DestroyRenderer(presenter.renderer);SDL_FreeSurface(target);
    rr_surface_destroy(game);rr_surface_destroy(screen);
#ifndef RR_SDL_GLES
    if (std::getenv("RR_OSD_GL_TEST")) {
        assert(SDL_InitSubSystem(SDL_INIT_VIDEO)==0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_Window* window=SDL_CreateWindow("OSD test",0,0,640,480,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
        assert(window);rr_context_t context{};context.display=&output;context.window=window;
        context.gl=SDL_GL_CreateContext(window);assert(context.gl);
        assert(ensure_post_pipeline(&context));
        glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,640,480);
        glClearColor(0.0f,0.0f,1.0f,1.0f);glClear(GL_COLOR_BUFFER_BIT);
        glBlendColor(0.125f,0.25f,0.5f,0.75f);glDisable(GL_BLEND);
        blit_overlay(&context,surface,0,0,640,480,640,480,RR_ROTATION_DEGREES_0,0,osd::theme().menuOpacity);
        unsigned char pixel[4]{};glReadPixels(620,280,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        const int bodyBlue=(osd::theme().colors.menuBody&31)*255/31;
        const int expected=(bodyBlue*osd::theme().menuOpacity+255*(255-osd::theme().menuOpacity))/255;
        assert(std::abs(static_cast<int>(pixel[2])-expected)<=3);
        GLfloat restored[4]{};glGetFloatv(GL_BLEND_COLOR,restored);
        assert(restored[0]==0.125f && restored[3]==0.75f && !glIsEnabled(GL_BLEND));
        if(context.overlay_texture)glDeleteTextures(1,&context.overlay_texture);
        glDeleteBuffers(1,&context.post_vbo);glDeleteVertexArrays(1,&context.post_vao);
        glDeleteProgram(context.post_program);SDL_GL_DeleteContext(context.gl);SDL_DestroyWindow(window);
    }
#endif
#endif
    rr_surface_destroy(surface);
    auto* keyboard=rr_surface_create(nullptr,INFO_MENU_WIDTH,INFO_MENU_HEIGHT,format_565);
    rr_keyboard_text_open("Text input","Example",false,[](const std::string&){});
    rr_keyboard_virtual_render(keyboard,INFO_MENU_WIDTH,INFO_MENU_HEIGHT);
    capture(keyboard,std::string(argv[1])+"/keyboard.ppm");
    rr_keyboard_virtual_close();rr_surface_destroy(keyboard);
    showLoading=false;isOpenGL=true;input_info_requested=false;input_fps_requested=false;
    input_slot_memory_plus_requested=true;
    uiRenderOverlays(nullptr,320,240,640);
    capture(uiCurrentOverlays()->bottom_center,std::string(argv[1])+"/slot.ppm");
    fps=60;showFPSImage();capture(status_surface_top_right,std::string(argv[1])+"/fps.ppm");
    return 0;
}
