#include "../src/config/catalog_precedence.h"
#include <cassert>
#include "../src/input/button_names.h"
int main(){
    assert(rr::configurableButtonName("SELECT") && rr::configurableButtonName("F12") && !rr::configurableButtonName("invalid") && !rr::configurableButtonName(""));
    std::map<std::string,std::string> profile{{"reicast_internal_resolution","640x480"},{"retrorun_audio_stable_buffer","true"},{"reicast_threaded_rendering","enabled"}};
    const auto original=profile;
    assert(rr::config_policy::preserveManualCatalogSettings(profile,{})==0 && profile==original);
    std::map<std::string,std::string> config{{"retrorun_flycast_manual_overrides","flycast2022_internal_resolution,retrorun_audio_stable_buffer"},{"flycast2022_internal_resolution","320x240"},{"retrorun_audio_stable_buffer","false"}};
    assert(rr::config_policy::preserveManualCatalogSettings(profile,config)==2);
    assert(profile.size()==1 && profile.count("reicast_threaded_rendering"));
    assert(config.at("flycast2022_internal_resolution")=="320x240");
    assert(rr::config_policy::canonicalCatalogKey("flycast2021le_internal_resolution")=="reicast_internal_resolution");
}
