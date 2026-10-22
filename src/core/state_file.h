#pragma once
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// One sidecar per actual state filename, including numbered/Flycast variants.
// Commit state first, then its optional preview. A failed capture must never
// leave a thumbnail belonging to the previous contents of the slot.
namespace state_file {
inline std::string previewPath(const std::string& state){return state+".png";}
inline std::string temporaryPattern(const std::string& path){
    const auto slash=path.find_last_of('/');
    return (slash==std::string::npos?std::string():path.substr(0,slash+1))+".rrstate-tmp-XXXXXX";
}
inline bool write(const std::string& path,const void* bytes,std::size_t size){
    if(path.empty() || !bytes || !size){errno=EINVAL;return false;}
    std::string temporary=temporaryPattern(path);int fd=mkstemp(temporary.data());if(fd<0)return false;
    bool ok=true;std::size_t used=0;
    while(used<size){const auto n=::write(fd,static_cast<const char*>(bytes)+used,size-used);if(n<0 && errno==EINTR)continue;if(n<=0){ok=false;break;}used+=static_cast<std::size_t>(n);}
    if(ok && fsync(fd))ok=false;
    if(close(fd))ok=false;
    if(ok && unlink(previewPath(path).c_str()) && errno!=ENOENT && errno!=ENAMETOOLONG)ok=false;
    if(ok && rename(temporary.c_str(),path.c_str()))ok=false;
    const int failure=errno;unlink(temporary.c_str());errno=failure;return ok;
}
template<class Capture> bool preview(const std::string& state,Capture capture){
    const auto path=previewPath(state);std::string temporary=temporaryPattern(path);
    int fd=mkstemp(temporary.data());if(fd<0)return false;
    if(close(fd)){unlink(temporary.c_str());return false;}
    bool ok=false;
    try{ok=capture(temporary.c_str());}catch(...){ok=false;}
    if(ok){
        fd=open(temporary.c_str(),O_RDWR);struct stat info{};
        ok=fd>=0 && !fstat(fd,&info) && info.st_size>0 && !fsync(fd);
        if(fd>=0 && close(fd))ok=false;
    }
    if(ok && rename(temporary.c_str(),path.c_str()))ok=false;
    unlink(temporary.c_str());return ok;
}
}
