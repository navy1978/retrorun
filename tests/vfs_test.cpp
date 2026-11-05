#include "core/vfs.h"
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <iostream>
void check(bool ok){if(!ok)throw std::runtime_error("VFS contract assertion failed");}
int main(){
 char temp[]="/tmp/rr-vfs-test-XXXXXX";const auto* folder=mkdtemp(temp);check(folder);const std::filesystem::path root=folder;
 try{
  retro_vfs_interface_info request{4,nullptr};check(!rr::vfs::provide(&request) && !request.iface);request.required_interface_version=1;check(rr::vfs::provide(&request) && request.required_interface_version==3);auto& v=*request.iface;
  const auto path=(root/"ROM with spaces.a26").string();auto* f=v.open(path.c_str(),RETRO_VFS_FILE_ACCESS_READ_WRITE,0);check(f);check(std::string(v.get_path(f))==path);
  check(v.write(f,"abcd",4)==4 && v.size(f)==4 && v.tell(f)==4);check(v.seek(f,1,RETRO_VFS_SEEK_POSITION_START)==1);char bytes[8]{};check(v.read(f,bytes,2)==2 && std::string(bytes,2)=="bc");check(v.close(f)==0);
  f=v.open(path.c_str(),RETRO_VFS_FILE_ACCESS_WRITE|RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING,0);check(f && v.size(f)==4);check(v.write(f,"XY",2)==2 && v.flush(f)==0);check(v.truncate(f,3)==0 && v.size(f)==3);check(v.close(f)==0);
  f=v.open(path.c_str(),RETRO_VFS_FILE_ACCESS_READ,0);check(f);check(v.read(f,bytes,8)==3 && std::string(bytes,3)=="XYc");check(v.close(f)==0);
  const auto large=(root/"sparse.bin").string();f=v.open(large.c_str(),RETRO_VFS_FILE_ACCESS_READ_WRITE,0);check(f);check(v.seek(f,3LL*1024*1024*1024,RETRO_VFS_SEEK_POSITION_START)==3LL*1024*1024*1024);check(v.write(f,"x",1)==1 && v.size(f)==3LL*1024*1024*1024+1);int32_t size=0;check(v.stat(large.c_str(),&size)==RETRO_VFS_STAT_IS_VALID && size==INT32_MAX);check(v.close(f)==0 && v.remove(large.c_str())==0);
  const auto sub=(root/"folder").string();check(v.mkdir(sub.c_str())==0 && v.mkdir(sub.c_str())==-2);check(v.stat(sub.c_str(),nullptr)&RETRO_VFS_STAT_IS_DIRECTORY);std::ofstream(root/".hidden")<<'x';
  auto* d=v.opendir(root.c_str(),false);check(d);std::set<std::string> names;while(v.readdir(d)){const auto name=std::string(v.dirent_get_name(d));names.insert(name);check(v.dirent_is_dir(d)==(name=="folder"));}check(!v.dirent_get_name(d) && v.closedir(d)==0);check(names==std::set<std::string>{"ROM with spaces.a26","folder"});
  d=v.opendir(root.c_str(),true);check(d);names.clear();while(v.readdir(d))names.insert(v.dirent_get_name(d));check(names.count(".hidden") && !names.count(".") && v.closedir(d)==0);
  check(!v.open(sub.c_str(),RETRO_VFS_FILE_ACCESS_READ,0));check(v.stat("/nonexistent-rr-fixture",nullptr)==0);check(v.close(nullptr)==-1);
  std::filesystem::remove_all(root);std::cout<<"VFS v1-v3 negotiation, ROM read, update/truncate, 64-bit files, directories and errors passed\n";
 }catch(...){std::filesystem::remove_all(root);throw;}
}
