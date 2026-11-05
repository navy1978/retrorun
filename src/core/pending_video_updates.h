#pragma once
#include "libretro.h"
#include <mutex>
namespace rr {
// Coalesce callbacks until retro_run returns. Geometry-only updates neither
// replace allocation maxima nor trigger audio/timing reinitialization.
class PendingVideoUpdates {
    std::mutex mutex;
    retro_system_av_info av{};
    retro_game_geometry geometry{};
    bool hasAv=false,hasGeometry=false;
    static void base(retro_game_geometry& to,const retro_game_geometry& from){
        to.base_width=from.base_width;to.base_height=from.base_height;to.aspect_ratio=from.aspect_ratio;
    }
public:
    void stageAv(const retro_system_av_info& value){std::lock_guard<std::mutex> lock(mutex);av=value;hasAv=true;hasGeometry=false;}
    bool stageGeometry(const retro_game_geometry& value){
        if(!value.base_width || !value.base_height)return false;
        std::lock_guard<std::mutex> lock(mutex);
        if(hasAv)base(av.geometry,value);else {geometry=value;hasGeometry=true;}return true;
    }
    bool takeAv(retro_system_av_info* out){if(!out)return false;std::lock_guard<std::mutex> lock(mutex);if(!hasAv)return false;*out=av;hasAv=false;return true;}
    bool takeGeometry(retro_game_geometry* out){if(!out)return false;std::lock_guard<std::mutex> lock(mutex);if(!hasGeometry)return false;*out=geometry;hasGeometry=false;return true;}
    void clear(){std::lock_guard<std::mutex> lock(mutex);av={};geometry={};hasAv=hasGeometry=false;}
};
}
