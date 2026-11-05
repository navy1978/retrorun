#include "config/profile_settings.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
namespace fs=std::filesystem;
void check(bool value){if(!value)throw std::runtime_error("Profile bridge assertion failed");}
int main(int argc,char** argv){
    if(argc>1 && std::string(argv[1])=="--operation"){
        const std::string value=argv[argc-1];if(value=="fail")return 1;if(value=="hang")sleep(5);
        std::ofstream out(std::getenv("RR_PROFILE_TEST_LOG"));for(int i=1;i<argc;++i)out<<argv[i]<<'\n';return 0;
    }
    char pattern[]="/tmp/rr-profile-test-XXXXXX";const auto* directory=mkdtemp(pattern);check(directory);const fs::path root=directory;
    try{
        const auto log=root/"arguments";setenv("RR_PROFILE_TEST_LOG",log.c_str(),1);
        std::map<std::string,std::string> config{{"retrorun_profile_mode","managed"},{"retrorun_profile_helper",fs::absolute(argv[0]).string()},{"retrorun_profile_systems","/systems with spaces.xml"},{"retrorun_profile_preferences","/preferences.xml"},{"retrorun_profile_system","snes"},{"retrorun_profile_rom","/games/$(literal) & name.sfc"},{"retrorun_profile_core","snes.so"}};
        check(rr::profiles::logPath(config,"/derived/runtime.cfg")=="/derived/retrorun.log");
        config["retrorun_profile_base"]="/base directory/retrorun.cfg";
        check(rr::profiles::logPath(config,"/derived/runtime.cfg")=="/base directory/retrorun.log");
        for(const auto* scope:{"game","system","core","global"}){
            check(rr::profiles::save(config,scope,"retrorun_pixel_perfect","true"));
            std::ifstream in(log);const std::string args{std::istreambuf_iterator<char>(in),{}};
            check(args.find(std::string("--scope\n")+scope+"\n")!=std::string::npos);
            check(args.find("/games/$(literal) & name.sfc\n")!=std::string::npos);
        }
        check(!rr::profiles::save(config,"game","key","fail"));
        check(!rr::profiles::save(config,"game","key","hang"));
        check(!rr::profiles::save(config,"global","retrorun_achievements_token","synthetic"));
        check(!rr::profiles::save(config,"invalid","key","true"));
        config.erase("retrorun_profile_mode");check(!rr::profiles::managed(config));
        check(!rr::profiles::save(config,"game","key","true"));
        config["retrorun_profile_mode"]="managed";config["retrorun_profile_helper"]="relative";
        check(!rr::profiles::save(config,"game","key","true"));
        fs::remove_all(root);std::cout<<"Managed bridge scopes, exact arguments, opt-in, failure and timeout passed\n";
    }catch(...){fs::remove_all(root);throw;}
}
