#pragma once
#include <cstdint>
namespace go2_profile {
enum class Stage { Map,Copy,Effect,FusedUpload,Unmap,QueueWait,RgaBlit,Submit,Frame,Count };
}
#if defined(RR_GO2_SOFTWARE_PROFILE) && !defined(RR_PLATFORM_SDL)
#include "diagnostics/benchmark.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <unistd.h>
namespace go2_profile {
using Clock=std::chrono::steady_clock;
constexpr unsigned stages=static_cast<unsigned>(Stage::Count),capacity=4096;
struct Metric {std::uint64_t count=0,total=0;std::array<std::uint64_t,capacity> recent{};};
struct Session {
    std::string path;std::mutex mutex;std::array<Metric,stages> metrics{};bool written=false,mixed=false;
    unsigned width=0,height=0,format=0,shader=0;
    explicit Session(const char* file):path(file){}
    void flush(){
        std::lock_guard<std::mutex> lock(mutex);if(written || !metrics[0].count)return;
        std::string tmp=path+".tmp.XXXXXX";int fd=mkstemp(tmp.data());if(fd<0)return;
        FILE* out=fdopen(fd,"w");if(!out){close(fd);unlink(tmp.c_str());return;}
        const char* names[]={"map","copy_conversion","legacy_effect","fused_upload","unmap","presenter_queue_wait","rga_blit","presenter_submit","software_callback"};
        std::fprintf(out,"{\"schema\":1,\"coordinates\":\"source\",\"p95_window\":\"last_4096_frames\",\"width\":%u,\"height\":%u,\"format\":%u,\"shader\":%u,\"mixed_geometry_or_mode\":%s,\"stages\":{",width,height,format,shader,mixed?"true":"false");
        for(unsigned i=0;i<stages;++i){const auto& m=metrics[i];const auto n=std::min<std::uint64_t>(m.count,capacity);auto sorted=m.recent;
            std::sort(sorted.begin(),sorted.begin()+n);const auto at=n?((n*95+99)/100)-1:0;
            std::fprintf(out,"%s\"%s\":{\"frames\":%llu,\"window_samples\":%llu,\"average_ms\":%.6f,\"p95_ms\":%.6f}",i?",":"",names[i],(unsigned long long)m.count,(unsigned long long)n,m.count?double(m.total)/m.count/1e6:0,n?sorted[at]/1e6:0);
        }
        std::fprintf(out,"}}\n");const bool good=!std::ferror(out) && std::fflush(out)==0 && fsync(fd)==0;
        const bool closed=std::fclose(out)==0;if(good && closed && !std::rename(tmp.c_str(),path.c_str()))written=true;else unlink(tmp.c_str());
    }
};
inline Session*& sessionStorage(){static Session* value=nullptr;return value;}
inline Session* session(){
    static const bool initialized=[] {const char* path=std::getenv("RETRORUN_GO2_SOFTWARE_PROFILE");if(path && *path && std::strlen(path)<4096){sessionStorage()=new(std::nothrow) Session(path);if(sessionStorage())std::atexit([]{sessionStorage()->flush();});}return true;}();
    (void)initialized;return sessionStorage();
}
struct Frame;inline thread_local Frame* active=nullptr;
struct Frame {
    Session* target=nullptr;Frame* previous=nullptr;Clock::time_point started;std::array<std::uint64_t,stages> totals;
    Frame(unsigned w,unsigned h,unsigned fmt,unsigned effect){
        target=session();if(!target || (benchmark_requested() && !benchmark_collecting())){target=nullptr;return;}
        previous=active;totals.fill(0);started=Clock::now();active=this;
        std::lock_guard<std::mutex> lock(target->mutex);
        if(!target->width){target->width=w;target->height=h;target->format=fmt;target->shader=effect;}
        else if(target->width!=w || target->height!=h || target->format!=fmt || target->shader!=effect)target->mixed=true;
    }
    ~Frame(){if(!target)return;totals[static_cast<unsigned>(Stage::Frame)]=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-started).count();active=previous;
        std::lock_guard<std::mutex> lock(target->mutex);for(unsigned i=0;i<stages;++i){auto& m=target->metrics[i];m.recent[m.count%capacity]=totals[i];++m.count;m.total+=totals[i];}}
};
struct Timer {
    Frame* frame;Stage stage;Clock::time_point started;
    explicit Timer(Stage s):frame(active),stage(s){if(frame)started=Clock::now();}
    void stop(){if(frame){frame->totals[static_cast<unsigned>(stage)]+=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-started).count();frame=nullptr;}}
    ~Timer(){stop();}
};
inline void finish(){if(sessionStorage())sessionStorage()->flush();}
}
#else
namespace go2_profile {
struct Frame {Frame(unsigned,unsigned,unsigned,unsigned){}};
struct Timer {explicit Timer(Stage){}void stop(){}};
inline void finish(){}
}
#endif
