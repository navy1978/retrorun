// SPDX-License-Identifier: MIT
#pragma once
#include "state_identity.h"
// Configured once on the emulation thread before any state/screenshot workers.
// No new references to frontend globals: pure file-format tests remain portable.
namespace rr::state_context {
inline state_identity::Record& current(){static state_identity::Record value;return value;}
inline void configure(const std::string& rom,const std::string& core){
 std::error_code error;const auto path=std::filesystem::canonical(rom,error);
 current()={{"romPath",error?"":path.string()},{"rom",state_identity::romKey(rom)},
            {"core",state_identity::hashFile(core)},{"platform",state_identity::platform()}};
}
inline std::string latestPath(const std::string& manual){return manual+".latest";}
inline bool record(const std::string& path,const std::string& kind,const void* bytes=nullptr,std::size_t size=0){
 if(current().empty())return false;
 auto data=current();data["kind"]=kind;
 if(bytes){data["bytes"]=std::to_string(size);data["hash"]=state_identity::hashBytes(bytes,size);}
 else{struct stat info{};if(stat(path.c_str(),&info) || !S_ISREG(info.st_mode))return false;
      data["bytes"]=std::to_string(info.st_size);data["hash"]=state_identity::hashFile(path);}
 if(state_identity::write(path,data))return true;
 // An old sidecar must never describe the new file after a partial metadata write.
 unlink(state_identity::sidecar(path).c_str());return false;
}
inline bool canLoad(const std::string& path,const void* bytes,std::size_t size,bool automatic){
 const auto data=state_identity::read(path);
 const auto status=state_identity::compatibility(data,state_identity::get(current(),"core"),state_identity::get(current(),"rom"));
 if(status=="incompatible" || (automatic && status!="compatible"))return false;
 const auto count=state_identity::get(data,"bytes"),hash=state_identity::get(data,"hash");
 if(!count.empty() && count!=std::to_string(size))return false;
 return hash.empty() || hash==state_identity::hashBytes(bytes,size);
}
}
