#pragma once
#include <array>
#include <algorithm>
#include <string>
namespace rr {
inline bool configurableButtonName(const std::string& name){
    static constexpr std::array<const char*,28> names{{"DPadUp","DPadDown","DPadLeft","DPadRight","A","B","X","Y","START","SELECT","TopLeft","TopRight","TriggerLeft","TriggerRight","THUMBL","THUMBR","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12"}};
    return std::any_of(names.begin(),names.end(),[&](const char* candidate){return name==candidate;});
}
}
