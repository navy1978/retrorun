#pragma once
#include "libretro.h"
#include <cstdio>
#include <algorithm>
#include <string>
#include <new>
#include <limits>
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

// Standard libretro VFS v3, backed by the same local POSIX files available to
// trusted cores. Opaque handles own their resources until close/closedir.
namespace rr::vfs {
struct File {FILE* stream;std::string path;bool writable;};
struct Directory {DIR* stream;dirent* entry=nullptr;bool hidden;};
inline File* file(retro_vfs_file_handle* p){return reinterpret_cast<File*>(p);}
inline Directory* directory(retro_vfs_dir_handle* p){return reinterpret_cast<Directory*>(p);}
inline const char* path(retro_vfs_file_handle* p){return p?file(p)->path.c_str():nullptr;}
inline retro_vfs_file_handle* open(const char* name,unsigned access,unsigned){
    if(!name || (access & ~7u))return nullptr;
    const unsigned mode=access&RETRO_VFS_FILE_ACCESS_READ_WRITE;
    if(!mode)return nullptr;
    FILE* stream=std::fopen(name,mode==RETRO_VFS_FILE_ACCESS_READ?"rb":access&RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING?"r+b":mode==RETRO_VFS_FILE_ACCESS_WRITE?"wb":"w+b");
    if(!stream)return nullptr;
    struct stat st{};if(fstat(fileno(stream),&st) || S_ISDIR(st.st_mode)){std::fclose(stream);return nullptr;}
    try{return reinterpret_cast<retro_vfs_file_handle*>(new File{stream,name,bool(mode&RETRO_VFS_FILE_ACCESS_WRITE)});}catch(...){std::fclose(stream);return nullptr;}
}
inline int close(retro_vfs_file_handle* p){if(!p)return -1;auto* f=file(p);const int result=std::fclose(f->stream);delete f;return result? -1:0;}
inline int64_t size(retro_vfs_file_handle* p){struct stat st{};if(!p || (file(p)->writable && std::fflush(file(p)->stream)))return -1;return !fstat(fileno(file(p)->stream),&st)?st.st_size:-1;}
inline int64_t tell(retro_vfs_file_handle* p){return p?ftello(file(p)->stream):-1;}
inline int64_t seek(retro_vfs_file_handle* p,int64_t offset,int origin){
    if(!p || origin<RETRO_VFS_SEEK_POSITION_START || origin>RETRO_VFS_SEEK_POSITION_END)return -1;
    const int origins[]={SEEK_SET,SEEK_CUR,SEEK_END};return fseeko(file(p)->stream,offset,origins[origin])? -1:tell(p);
}
inline int64_t read(retro_vfs_file_handle* p,void* data,uint64_t count){
    if(!p || (!data && count) || count>std::numeric_limits<size_t>::max() || count>INT64_MAX)return -1;
    auto* stream=file(p)->stream;const auto n=std::fread(data,1,count,stream);return n==0 && std::ferror(stream)?-1:static_cast<int64_t>(n);
}
inline int64_t write(retro_vfs_file_handle* p,const void* data,uint64_t count){
    if(!p || (!data && count) || count>std::numeric_limits<size_t>::max() || count>INT64_MAX)return -1;
    auto* stream=file(p)->stream;const auto n=std::fwrite(data,1,count,stream);return n==0 && std::ferror(stream)?-1:static_cast<int64_t>(n);
}
inline int flush(retro_vfs_file_handle* p){return p && !std::fflush(file(p)->stream)?0:-1;}
inline int remove(const char* name){return name && !std::remove(name)?0:-1;}
inline int rename(const char* from,const char* to){return from && to && !std::rename(from,to)?0:-1;}
inline int64_t truncate(retro_vfs_file_handle* p,int64_t length){return p && length>=0 && !std::fflush(file(p)->stream) && !ftruncate(fileno(file(p)->stream),length)?0:-1;}
inline int stat(const char* name,int32_t* bytes){
    struct ::stat st{};if(bytes)*bytes=0;if(!name || ::stat(name,&st))return 0;
    if(bytes)*bytes=static_cast<int32_t>(std::min<int64_t>(INT32_MAX,st.st_size));
    return RETRO_VFS_STAT_IS_VALID | (S_ISDIR(st.st_mode)?RETRO_VFS_STAT_IS_DIRECTORY:0) | (S_ISCHR(st.st_mode)?RETRO_VFS_STAT_IS_CHARACTER_SPECIAL:0);
}
inline int mkdir(const char* name){if(!name)return -1;return ::mkdir(name,0755)?(errno==EEXIST?-2:-1):0;}
inline retro_vfs_dir_handle* opendir(const char* name,bool hidden){
    if(!name)return nullptr;
    DIR* stream=::opendir(name);if(!stream)return nullptr;
    auto* d=new(std::nothrow) Directory{stream,nullptr,hidden};if(!d){::closedir(stream);return nullptr;}return reinterpret_cast<retro_vfs_dir_handle*>(d);
}
inline bool readdir(retro_vfs_dir_handle* p){
    if(!p)return false;
    auto* d=directory(p);
    while((d->entry=::readdir(d->stream))){const auto* name=d->entry->d_name;if(!std::strcmp(name,".") || !std::strcmp(name,"..") || (!d->hidden && name[0]=='.'))continue;return true;}return false;
}
inline const char* name(retro_vfs_dir_handle* p){return p && directory(p)->entry?directory(p)->entry->d_name:nullptr;}
inline bool isDirectory(retro_vfs_dir_handle* p){struct ::stat st{};return name(p) && !fstatat(dirfd(directory(p)->stream),name(p),&st,0) && S_ISDIR(st.st_mode);}
inline int closedir(retro_vfs_dir_handle* p){if(!p)return -1;auto* d=directory(p);const int result=::closedir(d->stream);delete d;return result? -1:0;}
inline bool provide(retro_vfs_interface_info* info){
    if(!info)return false;
    info->iface=nullptr;if(info->required_interface_version>3)return false;
    static retro_vfs_interface api{path,open,close,size,tell,seek,read,write,flush,remove,rename,truncate,stat,mkdir,opendir,readdir,name,isDirectory,closedir};
    info->required_interface_version=3;info->iface=&api;return true;
}
}
