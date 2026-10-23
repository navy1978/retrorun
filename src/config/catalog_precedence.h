#pragma once
#include <map>
#include <set>
#include <sstream>
#include <string>
namespace rr::config_policy {
inline std::string canonicalCatalogKey(const std::string& key){
    for(const auto* prefix:{"flycast2022_","flycast2021le_","flycast2021_","flycast_","reicast_"}){
        const std::string name(prefix);if(key.rfind(name,0)==0)return "reicast_"+key.substr(name.size());
    }
    return key;
}
// Explicit system/game choices win over optional catalog recommendations.
// A missing marker preserves standalone RetroRun's existing catalog behavior.
inline std::size_t preserveManualCatalogSettings(std::map<std::string,std::string>& catalog,const std::map<std::string,std::string>& configuration){
    const auto marker=configuration.find("retrorun_flycast_manual_overrides");
    if(marker==configuration.end() || marker->second.size()>65536)return 0;
    std::set<std::string> protectedKeys;std::istringstream input(marker->second);std::string key;
    while(std::getline(input,key,','))if(!key.empty())protectedKeys.insert(canonicalCatalogKey(key));
    std::size_t removed=0;
    for(auto it=catalog.begin();it!=catalog.end();)if(protectedKeys.count(canonicalCatalogKey(it->first))){it=catalog.erase(it);++removed;}else ++it;
    return removed;
}
}
