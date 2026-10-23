// SPDX-License-Identifier: MIT
// Shared file identity format with RetroRun src/core/state_identity.h.
// Fingerprints detect accidental core/content changes; they are NOT signatures
// and do not establish that an untrusted save state is safe to deserialize.
#pragma once
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>
#include <array>
#include <cstdint>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
namespace rr::state_identity {
using Record=std::map<std::string,std::string>;
inline std::string hashBytes(const void* data,std::size_t size,std::uint64_t value=14695981039346656037ULL){
 const auto* bytes=static_cast<const unsigned char*>(data);for(std::size_t i=0;i<size;++i){value^=bytes[i];value*=1099511628211ULL;}
 std::ostringstream out;out<<std::hex<<std::setw(16)<<std::setfill('0')<<value;return out.str();
}
inline std::string hashFile(const std::filesystem::path& path){
 struct stat info{};if(stat(path.c_str(),&info) || !S_ISREG(info.st_mode) || info.st_size<0 || info.st_size>1024LL*1024*1024)return {};
 std::ifstream input(path,std::ios::binary);if(!input)return {};std::array<char,32768> buffer{};std::uint64_t value=14695981039346656037ULL;
 while(input){input.read(buffer.data(),buffer.size());for(std::streamsize i=0;i<input.gcount();++i){value^=static_cast<unsigned char>(buffer[i]);value*=1099511628211ULL;}}
 if(!input.eof())return {};
 std::ostringstream out;out<<std::hex<<std::setw(16)<<std::setfill('0')<<value;return out.str();
}
inline std::string platform(){
#if defined(__aarch64__)
 return "aarch64";
#elif defined(__arm__)
 return "arm";
#elif defined(__x86_64__)
 return "x86_64";
#else
 return "unknown";
#endif
}
inline std::string romKey(const std::filesystem::path& path){
 struct stat info{};std::error_code error;const auto canonical=std::filesystem::canonical(path,error);
 if(error || stat(canonical.c_str(),&info) || !S_ISREG(info.st_mode))return {};
 // Path/stat identity is deliberately conservative, not a content checksum.
 // Disc descriptors refer to additional tracks: without a track manifest they
 // cannot supply verified content identity. Keep those states unverified.
 auto extension=canonical.extension().string();
 for(auto& c:extension)if(c>='A' && c<='Z')c=char(c-'A'+'a');
 if(extension==".cue" || extension==".gdi" || extension==".m3u")return {};
#if defined(__APPLE__)
 const auto modified=info.st_mtimespec,changed=info.st_ctimespec;
#else
 const auto modified=info.st_mtim,changed=info.st_ctim;
#endif
 const auto text=canonical.string()+"\n"+std::to_string(info.st_size)+"\n"+
     std::to_string(modified.tv_sec)+":"+std::to_string(modified.tv_nsec)+"\n"+
     std::to_string(changed.tv_sec)+":"+std::to_string(changed.tv_nsec);
 return hashBytes(text.data(),text.size());
}
inline std::filesystem::path sidecar(const std::filesystem::path& path){return path.string()+".identity";}
inline Record read(const std::filesystem::path& path){
 const auto meta=sidecar(path);const int fd=open(meta.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK);if(fd<0)return {};
 struct stat info{};if(fstat(fd,&info) || !S_ISREG(info.st_mode) || info.st_size<0 || info.st_size>16384){close(fd);return {};}
 std::array<char,16385> bytes{};std::size_t used=0;bool good=true;
 while(used<bytes.size()){const auto n=::read(fd,bytes.data()+used,bytes.size()-used);if(n<0 && errno==EINTR)continue;if(n<0){good=false;break;}if(!n)break;used+=static_cast<std::size_t>(n);}
 close(fd);if(!good || used>16384)return {};
 std::istringstream input(std::string(bytes.data(),used));std::string line;if(!std::getline(input,line) || line!="RRIDENTITY1")return {};Record result;
 while(std::getline(input,line)){
  std::istringstream row(line);std::string key,value;if(!(row>>key>>std::quoted(value)) || key.size()>64 || value.size()>4096 || !result.emplace(key,value).second)return {};
  row>>std::ws;if(!row.eof())return {};
 }
 return result;
}
inline bool write(const std::filesystem::path& path,const Record& record){
 std::ostringstream out;out<<"RRIDENTITY1\n";
 for(const auto& [key,value]:record){if(key.empty() || key.size()>64 || key.find_first_of(" \r\n\t")!=std::string::npos || value.size()>4096 || value.find_first_of("\r\n")!=std::string::npos)return false;out<<key<<' '<<std::quoted(value)<<'\n';}
 const auto text=out.str();if(text.size()>16384)return false;const auto destination=sidecar(path);std::string temporary=destination.string()+".XXXXXX";
 const int fd=mkstemp(temporary.data());if(fd<0)return false;std::size_t used=0;bool good=true;
 while(used<text.size()){const auto n=::write(fd,text.data()+used,text.size()-used);if(n<0 && errno==EINTR)continue;if(n<=0){good=false;break;}used+=n;}
 if(good && fsync(fd))good=false;
 if(close(fd))good=false;
 if(good && rename(temporary.c_str(),destination.c_str()))good=false;
 unlink(temporary.c_str());return good;
}
inline std::string get(const Record& record,const char* key){const auto found=record.find(key);return found==record.end()?"":found->second;}
inline std::string compatibility(const Record& record,const std::string& core,const std::string& rom){
 if(record.empty())return "unknown";
 if(!core.empty() && !get(record,"core").empty() && get(record,"core")!=core)return "incompatible";
 if(!get(record,"platform").empty() && get(record,"platform")!=platform())return "incompatible";
 if(!rom.empty() && !get(record,"rom").empty() && get(record,"rom")!=rom)return "incompatible";
 if(core.empty() || rom.empty())return "unknown";
 for(const auto* key:{"core","rom","platform","bytes","hash"})if(get(record,key).empty())return "unknown";
 return "compatible";
}
}
