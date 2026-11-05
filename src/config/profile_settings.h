// Opt-in bridge to a launcher-owned profile store. No shell or legacy fallback.
#pragma once
#include <map>
#include <filesystem>
#include <string>
#include <vector>
#include <chrono>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cerrno>
namespace rr::profiles {
inline bool managed(const std::map<std::string,std::string>& values){
    const auto it=values.find("retrorun_profile_mode");return it!=values.end() && it->second=="managed";
}
inline std::filesystem::path logPath(const std::map<std::string,std::string>& values,const std::filesystem::path& active){
    const auto base=values.find("retrorun_profile_base");
    const auto config=managed(values) && base!=values.end() && !base->second.empty()?std::filesystem::path(base->second):active;
    return config.parent_path()/"retrorun.log";
}
inline bool save(const std::map<std::string,std::string>& values,const std::string& scope,const std::string& key,const std::string& value){
    if(key=="retrorun_achievements_username" || key=="retrorun_achievements_password" || key=="retrorun_achievements_token")return false;
    if(!managed(values) || (scope!="game" && scope!="system" && scope!="core" && scope!="global"))return false;
    auto get=[&](const char* name){auto it=values.find(std::string("retrorun_profile_")+name);return it==values.end()?std::string{}:it->second;};
    std::vector<std::string> command{get("helper"),"--operation","set-setting","--systems",get("systems"),"--preferences",get("preferences"),"--system",get("system"),"--rom",get("rom"),"--core",get("core"),"--scope",scope,"--key",key,"--value",value};
    for(unsigned i:{0u,4u,6u})if(command[i].empty() || command[i].front()!='/')return false;
    if(command[8].empty() || command[10].empty() || command[12].empty())return false;
    for(const auto& argument:command)if(argument.size()>8192 || argument.find('\0')!=std::string::npos)return false;
    std::vector<char*> args;for(auto& item:command)args.push_back(item.data());args.push_back(nullptr);
    const int nullfd=open("/dev/null",O_RDWR|O_CLOEXEC);if(nullfd<0)return false;
    const pid_t child=fork();
    if(child==0){dup2(nullfd,STDIN_FILENO);dup2(nullfd,STDOUT_FILENO);dup2(nullfd,STDERR_FILENO);close(nullfd);execv(args[0],args.data());_exit(127);}
    close(nullfd);if(child<0)return false;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    int status=0;
    for(;;){const auto result=waitpid(child,&status,WNOHANG);if(result==child)return WIFEXITED(status) && WEXITSTATUS(status)==0;if(result<0 && errno!=EINTR)return false;
        if(std::chrono::steady_clock::now()>=deadline){kill(child,SIGKILL);while(waitpid(child,&status,0)<0 && errno==EINTR){}return false;}usleep(10000);
    }
}
}
