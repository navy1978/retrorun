// Original diagnostic libretro core: colored test frame, no emulator/game/BIOS.
// Explicit developer build only, never bundled as a console core.
#include "libretro.h"
#include <cstdint>
#include <cstring>
#if PREVIEW_TEST_HW
#include <GLES2/gl2.h>
#endif
namespace {
retro_environment_t environment;
retro_video_refresh_t video;
retro_input_poll_t poll;
retro_audio_sample_batch_t audio;
retro_hw_render_callback hardware{};
std::uint64_t frames=0;
std::uint32_t pixels[640*480];
void resetContext(){}
}
extern "C" {
unsigned retro_api_version(){return RETRO_API_VERSION;}
void retro_set_environment(retro_environment_t callback){environment=callback;}
void retro_set_video_refresh(retro_video_refresh_t callback){video=callback;}
void retro_set_audio_sample(retro_audio_sample_t){}
void retro_set_audio_sample_batch(retro_audio_sample_batch_t callback){audio=callback;}
void retro_set_input_poll(retro_input_poll_t callback){poll=callback;}
void retro_set_input_state(retro_input_state_t){}
void retro_init(){}
void retro_deinit(){}
void retro_get_system_info(retro_system_info* info){*info={"RetroRun state-preview fixture","1","bin",false,false};}
void retro_get_system_av_info(retro_system_av_info* info){*info={{640,480,640,480,4.0f/3.0f},{60.0,48000.0}};}
void retro_set_controller_port_device(unsigned,unsigned){}
void retro_reset(){frames=0;}
bool retro_load_game(const retro_game_info*){
#if PREVIEW_TEST_HW
    hardware.context_type=RETRO_HW_CONTEXT_OPENGLES2;hardware.context_reset=resetContext;hardware.bottom_left_origin=true;
    return environment(RETRO_ENVIRONMENT_SET_HW_RENDER,&hardware);
#else
    retro_pixel_format format=RETRO_PIXEL_FORMAT_XRGB8888;
    return environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT,&format);
#endif
}
bool retro_load_game_special(unsigned,const retro_game_info*,size_t){return false;}
void retro_unload_game(){}
void retro_run(){
    if(poll)poll();++frames;
#if PREVIEW_TEST_HW
    glBindFramebuffer(GL_FRAMEBUFFER,static_cast<GLuint>(hardware.get_current_framebuffer()));
    glViewport(0,0,640,480);glClearColor(0.125f,0.75f,0.25f,1.0f);glClear(GL_COLOR_BUFFER_BIT);
    video(RETRO_HW_FRAME_BUFFER_VALID,640,480,0);
#else
    for(unsigned y=0;y<480;++y)for(unsigned x=0;x<640;++x)pixels[y*640+x]=((x/80)%2)?0x00c04020:0x0020c040;
    video(pixels,640,480,640*4);
#endif
    if(audio){static const int16_t silence[1600]{};audio(silence,800);}
}
size_t retro_serialize_size(){return sizeof(frames);}
bool retro_serialize(void* data,size_t size){if(size!=sizeof(frames))return false;std::memcpy(data,&frames,size);return true;}
bool retro_unserialize(const void* data,size_t size){if(size!=sizeof(frames))return false;std::memcpy(&frames,data,size);return true;}
void retro_cheat_reset(){}
void retro_cheat_set(unsigned,bool,const char*){}
unsigned retro_get_region(){return RETRO_REGION_NTSC;}
void* retro_get_memory_data(unsigned){return nullptr;}
size_t retro_get_memory_size(unsigned){return 0;}
}
