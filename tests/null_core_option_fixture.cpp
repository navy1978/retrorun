// Libretro regression fixture: run with any original nonempty content file.
// Correct frontend behavior is false/null for an unavailable option key.
#include "libretro.h"
#include <cstdio>
#include <cstdlib>
static retro_environment_t environment;
extern "C" {
void retro_set_environment(retro_environment_t cb){environment=cb;}
void retro_set_video_refresh(retro_video_refresh_t){}
void retro_set_audio_sample(retro_audio_sample_t){}
void retro_set_audio_sample_batch(retro_audio_sample_batch_t){}
void retro_set_input_poll(retro_input_poll_t){}
void retro_set_input_state(retro_input_state_t){}
void retro_init(){retro_variable missing{nullptr,"stale"};if(environment(RETRO_ENVIRONMENT_GET_VARIABLE,&missing) || missing.value || environment(RETRO_ENVIRONMENT_GET_VARIABLE,nullptr))std::abort();retro_variable unknown{"__retrorun_missing_option__","stale"};if(environment(RETRO_ENVIRONMENT_GET_VARIABLE,&unknown) || unknown.value)std::abort();std::fputs("NULL_CORE_OPTION_FIXTURE_PASS\n",stderr);}
void retro_deinit(){}
unsigned retro_api_version(){return RETRO_API_VERSION;}
void retro_get_system_info(retro_system_info* out){*out={"Null option fixture","1","bin",true,false};}
void retro_get_system_av_info(retro_system_av_info* out){*out={{64,64,64,64,1.f},{60.,48000.}};}
void retro_set_controller_port_device(unsigned,unsigned){}
void retro_reset(){}
void retro_run(){}
bool retro_load_game(const retro_game_info*){return false;} // Stop after the assertion, without gameplay.
bool retro_load_game_special(unsigned,const retro_game_info*,size_t){return false;}
void retro_unload_game(){}
size_t retro_serialize_size(){return 0;}
bool retro_serialize(void*,size_t){return false;}
bool retro_unserialize(const void*,size_t){return false;}
void retro_cheat_reset(){}
void retro_cheat_set(unsigned,bool,const char*){}
unsigned retro_get_region(){return RETRO_REGION_NTSC;}
void* retro_get_memory_data(unsigned){return nullptr;}
size_t retro_get_memory_size(unsigned){return 0;}
}
