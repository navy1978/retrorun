// Off-screen render fixture: real OSD renderers, no emulator or display needed.
#include "globals.h"
#include "video-helper.h"
#include "ui-renderer.h"
#include "fonts.h"
#include "input.h"
#include "status.h"
#include "osd-theme.h"
#include "screenshot_capture.h"
#include "config/flycast_game_catalog.h"
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
#ifdef RR_OSD_SDL_PROBE
static void controllerMappingProbe(const char* name,bool handheld){
    SDL_VirtualJoystickDesc description{};description.version=SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    description.type=SDL_JOYSTICK_TYPE_GAMECONTROLLER;description.nbuttons=16;description.naxes=4;description.nhats=1;description.name=name;
    const int index=SDL_JoystickAttachVirtualEx(&description);assert(index>=0);
    char guid[33]{};SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index),guid,sizeof(guid));
    const std::string mapping=std::string(guid)+","+name+(handheld?",a:b1,b:b0,x:b2,y:b3,":",a:b0,b:b1,x:b2,y:b3,")+"back:b12,start:b13,leftx:a0,lefty:a1,";
    assert(SDL_GameControllerAddMapping(mapping.c_str())>=0);
    rr_input input{SDL_GameControllerOpen(index)};assert(input.controller);
    auto* joystick=SDL_GameControllerGetJoystick(input.controller);
    const rr_input_button_t expectedHandheld[]={RRInputButton_B,RRInputButton_A,RRInputButton_X,RRInputButton_Y};
    const rr_input_button_t expectedDesktop[]={RRInputButton_B,RRInputButton_A,RRInputButton_Y,RRInputButton_X};
    for(int button=0;button<4;++button){
        assert(SDL_JoystickSetVirtualButton(joystick,button,1)==0);SDL_JoystickUpdate();
        rr_input_state state{};rr_input_state_read(&input,&state);
        const auto expected=handheld?expectedHandheld[button]:expectedDesktop[button];
        for(int other=0;other<4;++other){const auto candidate=expectedHandheld[other];
            assert((rr_input_state_button_get(&state,candidate)==RRButtonState_Pressed)==(candidate==expected));}
        SDL_JoystickSetVirtualButton(joystick,button,0);SDL_JoystickUpdate();
    }
    SDL_GameControllerClose(input.controller);assert(SDL_JoystickDetachVirtual(index)==0);
}
#endif
int main(int argc,char** argv) {
    assert(argc==2 || argc==3);
    coreName="Flycast 2022 LE";assert(isFlycast2022() && isFlycast2021());
    {
        using namespace rr::flycast_profiles;Profile profile;bool fallback=false;
        assert(selectProfile(builtinCatalog(),"T1401N",Mode::BestPerformance,profile,fallback,"RG351MP","RK3326"));
        const auto settings=settingsForOptionPrefix(profile.settings,isFlycast2022()?"flycast2022_":"reicast_");
        assert(!fallback && settings.at("flycast2022_audio_mixer")=="lowend");
        assert(settings.at("flycast2022_fast_depth")=="vertex_fast_log");
        assert(settings.at("flycast2022_opaque_strip_merge")=="enabled");
        assert(settings.count("reicast_audio_mixer")==0);
    }
    coreName="Flycast 2022 Low-End";assert(isFlycast2022() && isFlycast2021());
    coreName="Flycast 2021";assert(!isFlycast2022() && isFlycast2021());coreName.clear();
#ifdef RR_OSD_SDL_PROBE
    assert(SDL_InitSubSystem(SDL_INIT_EVENTS|SDL_INIT_JOYSTICK|SDL_INIT_GAMECONTROLLER)==0);
    controllerMappingProbe("GO-Super Gamepad",true);
    controllerMappingProbe("Desktop Fixture",false);
#endif
    if(argc==3) osd::select(argv[2]);std::filesystem::create_directories(argv[1]);
    setUIProfile(UIProfile::Handheld);updateUIMenuDimensions(640,480);
    Menu menu("Video",{MenuItem("Integer Scale",enabled,[](int){},"bool"),MenuItem("Aspect Ratio",enabled,[](int){},"")});
    menu.resetSelected(0);menuManager.setCurrentMenu(&menu);
    auto* surface=rr_surface_create(nullptr,INFO_MENU_WIDTH,INFO_MENU_HEIGHT,format_565);
    drawMenuInfoBackgroud(surface,INFO_MENU_WIDTH,INFO_MENU_HEIGHT);showInfo(640,&surface);
    capture(surface,std::string(argv[1])+"/menu.ppm",true);
#ifdef RR_OSD_SDL_PROBE
    // NES software cores commonly supply XRGB8888 with zero in the unused
    // high byte. It is not transparent ARGB: PNG capture must retain RGB.
    rr_display_t softwareOutput{};
    auto* softwareSource=rr_surface_create(&softwareOutput,256,256,RR_PIXEL_FORMAT_XRGB8888);
    auto* softwarePixels=static_cast<uint32_t*>(rr_surface_map(softwareSource));
    std::fill(softwarePixels,softwarePixels+256*256,0x00ff0000u);rr_surface_unmap(softwareSource);
    const auto softwarePath=std::string(argv[1])+"/sdl-xrgb-capture.png";
    assert(captureSurfacePng(&softwareOutput,softwareSource,0,0,256,240,softwarePath.c_str(),0,0,RR_ROTATION_DEGREES_0));
    png_image softwareImage{};softwareImage.version=PNG_IMAGE_VERSION;
    assert(png_image_begin_read_from_file(&softwareImage,softwarePath.c_str()));softwareImage.format=PNG_FORMAT_RGBA;
    std::vector<unsigned char> softwareRgba(PNG_IMAGE_SIZE(softwareImage));
    assert(png_image_finish_read(&softwareImage,nullptr,softwareRgba.data(),0,nullptr));
    assert(softwareImage.width==256 && softwareImage.height==240);
    assert(softwareRgba[0]==255 && softwareRgba[1]==0 && softwareRgba[2]==0 && softwareRgba[3]==255);
    png_image_free(&softwareImage);rr_surface_destroy(softwareSource);
    auto* screen=rr_surface_create(nullptr,640,480,format_565);
    auto* game=rr_surface_create(nullptr,320,240,format_565);
    auto* gamePixels=static_cast<uint16_t*>(rr_surface_map(game));
    for(int y=0;y<240;++y)for(int x=0;x<320;++x)
        gamePixels[y*320+x]=(x/32+y/32)%2 ? osd::rgb565(40,80,200) : osd::rgb565(20,120,180);
    rr_surface_unmap(game);
    SDL_Surface* target=SDL_CreateRGBSurfaceWithFormatFrom(screen->pixels.data(),640,480,16,screen->stride,SDL_PIXELFORMAT_RGB565);
    assert(target);rr_display_t output{};output.width=640;output.height=480;
    rr_presenter_t presenter{};presenter.display=&output;presenter.renderer=SDL_CreateSoftwareRenderer(target);
    // Exercise the actual SDL screenshot and slot-preview writer, without ROMs.
    ::display=&output;::surface=game;screenShotFolder=argv[1];romName="sdl-capture-fixture";
    currentWidth=320;currentHeight=240;
    const auto previewPath=std::string(argv[1])+"/sdl-slot-preview.png";
    assert(saveStateScreenshot(previewPath.c_str()));
    screenshot_requested=true;takeScreenshot(0,0);assert(!screenshot_requested);
    bool captured=false;for(const auto& file:std::filesystem::directory_iterator(argv[1])){
        if(file.path().filename().string().find("sdl-capture-fixture-")==0){
            png_image image{};image.version=PNG_IMAGE_VERSION;
            assert(png_image_begin_read_from_file(&image,file.path().c_str()));
            assert(image.width==320 && image.height==240);png_image_free(&image);captured=true;
        }
    }
    assert(captured);::surface=nullptr;::display=nullptr;
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
        // Capture a core-owned FBO while preserving the presenter's default
        // read framebuffer and GL_BACK buffer. No game or menu pixels needed.
        context.width=640;context.height=480;
        GLuint captureTexture=0;glGenTextures(1,&captureTexture);glBindTexture(GL_TEXTURE_2D,captureTexture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,640,480,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glGenFramebuffers(1,&context.framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,context.framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,captureTexture,0);
        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
        glClearColor(1,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER,0);glReadBuffer(GL_BACK);
        while(glGetError()!=GL_NO_ERROR){}
        auto* glCapture=rr_context_surface_lock(&context);assert(glCapture);
        assert(glGetError()==GL_NO_ERROR);
        GLint restoredRead=1;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&restoredRead);assert(restoredRead==0);
        glGetIntegerv(GL_READ_BUFFER,&restoredRead);assert(restoredRead==GL_BACK);
        const auto glPath=std::string(argv[1])+"/sdl-gl-capture.png";
        assert(captureSurfacePng(&output,glCapture,0,0,640,480,glPath.c_str(),0,0,RR_ROTATION_DEGREES_0));
        auto* rgba=static_cast<unsigned char*>(rr_surface_map(glCapture));assert(rgba[0]==255 && rgba[1]==0 && rgba[2]==0);
        rr_surface_unmap(glCapture);rr_context_surface_unlock(&context,glCapture);
        glDeleteFramebuffers(1,&context.framebuffer);glDeleteTextures(1,&captureTexture);
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
    showLoading=true;isOpenGL=true;input_info_requested=false;input_fps_requested=false;
    uiRenderOverlays(nullptr,320,240,640);
    capture(uiCurrentOverlays()->full,std::string(argv[1])+"/loading-a.ppm");
    uiNotifyLoadingPresented();SDL_Delay(380);
    uiRenderOverlays(nullptr,320,240,640);
    capture(uiCurrentOverlays()->full,std::string(argv[1])+"/loading-b.ppm");
    if(osd::theme().minimalLoading){
        const auto read=[](const std::string& path){std::ifstream f(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});};
        assert(read(std::string(argv[1])+"/loading-a.ppm")!=read(std::string(argv[1])+"/loading-b.ppm"));
    }
    showLoading=false;isOpenGL=true;input_info_requested=false;input_fps_requested=false;
    input_slot_memory_plus_requested=true;
    uiRenderOverlays(nullptr,320,240,640);
    capture(uiCurrentOverlays()->bottom_center,std::string(argv[1])+"/slot.ppm");
    fps=60;showFPSImage();capture(status_surface_top_right,std::string(argv[1])+"/fps.ppm");
    return 0;
}
