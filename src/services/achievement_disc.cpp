// SPDX-License-Identifier: GPL-2.0-or-later
// CDI layout follows Flycast's core/deps/chdpsr/cdipsr.cpp.
// Reads are checked and integers decoded explicitly instead of reading into longs.
#include "achievement_disc.h"
#include <libchdr/chd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace rr { namespace achievements {
namespace {
struct HashContext {
    const std::atomic<bool>* cancel;
    std::string error;
    bool io_failed = false;
    bool dreamcast_cdi = false;
    std::string reader_error;
    int attempt_index = -2;
};
void RC_CCONV hash_error(const char*, const rc_hash_iterator_t*);
HashContext* hash_context(const rc_hash_iterator_t* iterator) {
    // rc_client uses userdata for its own load state. Only our standalone
    // iterator pairs this callback with a HashContext; never cast client data.
    auto* context = iterator->callbacks.error_message == hash_error
        ? static_cast<HashContext*>(iterator->userdata) : nullptr;
    if (context && context->attempt_index != iterator->index) {
        // Failed probes for other consoles must not invalidate the next
        // attempt. rc_hash_iterate advances index before each algorithm.
        context->attempt_index = iterator->index;
        context->error.clear();
        context->reader_error.clear();
        context->io_failed = false;
    }
    return context;
}
struct Track {
    uint32_t number = 0, session = 0, start = 0, frames = 0, sector_size = 2352, header = 16;
    uint64_t offset = 0; // bytes in CDI, frames in CHD
    bool audio = false;
};
struct Handle {
    rc_hash_cdreader_t fallback = {};
    void* fallback_handle = nullptr;
    chd_file* chd = nullptr;
    FILE* file = nullptr;
    Track track;
    std::vector<uint8_t> hunk;
    uint32_t loaded_hunk = UINT32_MAX, unit = 2448;
    HashContext* context = nullptr;
    ~Handle() {
        if (fallback_handle) fallback.close_track(fallback_handle);
        if (chd) chd_close(chd);
        if (file) std::fclose(file);
    }
    bool cancelled() const {
        return context && context->cancel && context->cancel->load(std::memory_order_relaxed);
    }
};
std::string extension(const std::string& path) {
    const auto dot = path.find_last_of('.');
    std::string ext = dot == std::string::npos ? "" : path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    return ext;
}
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void set_error(const rc_hash_iterator_t* it, const char* message) {
    if (auto* context = hash_context(it)) {
        if (std::strcmp(message,"Requested disc track is unavailable"))
            context->reader_error = message;
    }
    if (it->callbacks.error_message) it->callbacks.error_message(message, it);
}
Track select_track(const std::vector<Track>& tracks, uint32_t requested) {
    require(!tracks.empty(), "Disc contains no tracks");
    if (requested == RC_HASH_CDTRACK_LAST) return tracks.back();
    const Track* selected = nullptr;
    for (const auto& t : tracks) {
        if (requested == t.number) return t;
        if (requested == RC_HASH_CDTRACK_FIRST_DATA && !t.audio) return t;
        if ((requested == 0 || requested == RC_HASH_CDTRACK_LARGEST) && !t.audio &&
            (!selected || t.frames > selected->frames)) selected = &t;
    }
    require(selected != nullptr, "Requested disc track is unavailable");
    return *selected;
}
uint32_t data_header(const char* type) {
    if (!std::strcmp(type, "MODE1_RAW") || !std::strcmp(type, "MODE1/2352")) return 16;
    if (!std::strcmp(type, "MODE2_RAW") || !std::strcmp(type, "MODE2/2352")) return 24;
    if (!std::strcmp(type, "MODE2") || !std::strcmp(type, "MODE2/2336")) return 8;
    if (!std::strcmp(type, "MODE1") || !std::strcmp(type, "MODE1/2048") ||
        !std::strcmp(type, "MODE2_FORM1") || !std::strcmp(type, "AUDIO")) return 0;
    throw std::runtime_error("Unsupported CHD track type");
}
std::vector<Track> chd_tracks(Handle& h) {
    std::vector<Track> tracks;
    uint64_t stored = 0;
    int64_t logical = 0;
    for (uint32_t i = 0; i < 99; ++i) {
        char meta[512] = {}, type[32] = {}, subtype[32] = {}, pgtype[32] = {}, pgsub[32] = {};
        unsigned number=0, frames=0, pregap=0, postgap=0, pad=0;
        int fields = 0;
        uint32_t length = 0;
        chd_error err = chd_get_metadata(h.chd, CDROM_TRACK_METADATA2_TAG, i, meta, sizeof(meta)-1, &length, nullptr, nullptr);
        if (err == CHDERR_NONE) {
            fields = std::sscanf(meta, "TRACK:%u TYPE:%31s SUBTYPE:%31s FRAMES:%u PREGAP:%u PGTYPE:%31s PGSUB:%31s POSTGAP:%u", &number,type,subtype,&frames,&pregap,pgtype,pgsub,&postgap);
            require(fields == 8, "Malformed CHD CD metadata");
        } else {
            err = chd_get_metadata(h.chd, CDROM_TRACK_METADATA_TAG, i, meta, sizeof(meta)-1, &length, nullptr, nullptr);
            if (err == CHDERR_NONE) {
                fields = std::sscanf(meta,"TRACK:%u TYPE:%31s SUBTYPE:%31s FRAMES:%u", &number,type,subtype,&frames);
                require(fields == 4, "Malformed CHD track metadata");
            } else {
                err = chd_get_metadata(h.chd, GDROM_TRACK_METADATA_TAG, i, meta, sizeof(meta)-1, &length, nullptr, nullptr);
                if (err == CHDERR_METADATA_NOT_FOUND)
                    err = chd_get_metadata(h.chd, GDROM_OLD_METADATA_TAG, i, meta, sizeof(meta)-1, &length, nullptr, nullptr);
                if (err == CHDERR_METADATA_NOT_FOUND) break;
                require(err == CHDERR_NONE, "Cannot read CHD metadata");
                fields = std::sscanf(meta,"TRACK:%u TYPE:%31s SUBTYPE:%31s FRAMES:%u PAD:%u PREGAP:%u PGTYPE:%31s PGSUB:%31s POSTGAP:%u", &number,type,subtype,&frames,&pad,&pregap,pgtype,pgsub,&postgap);
                require(fields == 9, "Malformed CHD GD metadata");
            }
        }
        require(length < sizeof(meta) && number == i+1 && frames > 0 && (pgtype[0] != 'V' || pregap <= frames) && pad <= frames,
                "Invalid CHD track extent");
        const bool stored_pregap = pgtype[0] == 'V';
        const uint32_t skip = stored_pregap ? pregap : 0;
        if (i == 0) logical = -static_cast<int64_t>(pregap);
        const int64_t start = logical + pregap;
        require(start >= 0 && start <= UINT32_MAX, "Invalid CHD track LBA");
        Track t;
        t.number=number; t.start=static_cast<uint32_t>(start); t.frames=frames-skip;
        t.offset=stored+skip; t.audio=!std::strcmp(type,"AUDIO"); t.header=data_header(type);
        tracks.push_back(t);
        stored += (uint64_t(frames)+3) & ~uint64_t(3);
        logical += frames + uint64_t(postgap) + (stored_pregap ? 0 : pregap);
        require(stored <= chd_get_header(h.chd)->logicalbytes/h.unit, "CHD tracks exceed image size");
    }
    return tracks;
}
bool chd_frame(Handle& h, uint64_t frame, const uint8_t*& data) {
    const uint64_t byte = frame * h.unit;
    const uint64_t number = byte / h.hunk.size();
    if (number > UINT32_MAX || byte >= chd_get_header(h.chd)->logicalbytes || h.cancelled()) return false;
    if (h.loaded_hunk != number) {
        const auto error = chd_read(h.chd, static_cast<uint32_t>(number), h.hunk.data());
        if (error != CHDERR_NONE) {
            if (h.context) {
                h.context->error = std::string("CHD decompression failed: ") + chd_error_string(error);
                h.context->io_failed = true;
            }
            return false;
        }
        h.loaded_hunk = static_cast<uint32_t>(number);
    }
    data = h.hunk.data() + byte % h.hunk.size();
    return true;
}
void open_chd(Handle& h, const char* path, uint32_t requested) {
    const auto err = chd_open(path, CHD_OPEN_READ, nullptr, &h.chd);
    if (err != CHDERR_NONE) throw std::runtime_error(std::string("Cannot open CHD: ")+chd_error_string(err));
    const auto* header = chd_get_header(h.chd);
    h.unit = header->unitbytes;
    require(h.unit == 2448 && header->hunkbytes && header->hunkbytes <= 16*1024*1024 && header->hunkbytes % h.unit == 0,
            "Unsupported CHD sector/hunk size");
    h.hunk.resize(header->hunkbytes);
    h.track = select_track(chd_tracks(h), requested);
}
// CDI descriptors live at the end of the image. Read only that bounded region.
struct Cursor {
    std::vector<uint8_t> bytes;
    size_t pos = 0;
    void skip(size_t count) { require(count <= bytes.size()-pos,"Truncated CDI descriptor"); pos += count; }
    uint32_t read(unsigned count) {
        require(count <= 4 && count <= bytes.size()-pos,"Truncated CDI descriptor");
        uint32_t value=0;
        for (unsigned i=0;i<count;++i) value |= uint32_t(bytes[pos++])<<(i*8);
        return value;
    }
};
void open_cdi(Handle& h, const char* path, uint32_t requested) {
    h.file=std::fopen(path,"rb"); require(h.file != nullptr,"Cannot open CDI");
    require(fseeko(h.file,0,SEEK_END)==0,"Cannot seek CDI");
    const auto size=ftello(h.file); require(size>=8,"Truncated CDI image");
    std::array<uint8_t,8> footer{};
    require(fseeko(h.file,size-8,SEEK_SET)==0 && std::fread(footer.data(),1,8,h.file)==8,"Cannot read CDI footer");
    Cursor foot; foot.bytes.assign(footer.begin(),footer.end());
    const auto version=foot.read(4), offset=foot.read(4);
    require(version>=0x80000004 && version<=0x80000006 && offset>0,"Unsupported CDI version");
    const uint64_t start=version==0x80000006 ? uint64_t(size)-offset : offset;
    require(start<uint64_t(size)-8 && uint64_t(size)-8-start<=1024*1024,"Invalid CDI descriptor offset");
    Cursor c; c.bytes.resize(uint64_t(size)-8-start);
    require(fseeko(h.file,static_cast<off_t>(start),SEEK_SET)==0 && std::fread(c.bytes.data(),1,c.bytes.size(),h.file)==c.bytes.size(),"Cannot read CDI descriptor");
    const auto sessions=c.read(2); require(sessions>0 && sessions<=99,"Invalid CDI session count");
    std::vector<Track> tracks;
    uint64_t position=0;
    for (uint32_t session=0;session<sessions;++session) {
        const auto count=c.read(2); require(count<=99-tracks.size(),"Invalid CDI track count");
        for (uint32_t i=0;i<count;++i) {
            if (c.read(4)) c.skip(8);
            constexpr uint8_t mark[10]={0,0,1,0,0,0,255,255,255,255};
            for (int m=0;m<2;++m) for (auto byte:mark) require(c.read(1)==byte,"Invalid CDI track marker");
            c.skip(4); const auto name_length=c.read(1); c.skip(name_length); c.skip(19);
            if (c.read(4)==0x80000000) c.skip(8);
            c.skip(2); const auto pregap=c.read(4), frames=c.read(4);
            c.skip(6); const auto mode=c.read(4); c.skip(12);
            const auto lba=c.read(4), total=c.read(4); c.skip(16);
            const auto format=c.read(4);
            require(mode<=2 && (format<=2 || format==4),"Unsupported CDI sector format");
            const uint32_t sector=format==0?2048:format==1?2336:format==2?2352:2448;
            require(frames>0 && uint64_t(frames)+pregap<=total && position+uint64_t(total)*sector<=start,
                    "CDI track exceeds image data");
            require(uint64_t(lba)+pregap>=150 && uint64_t(lba)+pregap+frames-150<=UINT32_MAX,"Invalid CDI track LBA");
            Track t; t.number=static_cast<uint32_t>(tracks.size()+1); t.session=session; t.frames=frames;
            t.start=lba+pregap-150; t.audio=mode==0; t.sector_size=sector;
            t.offset=position+uint64_t(pregap)*sector;
            t.header=sector==2048?0:sector==2336?8:mode==1?16:24;
            tracks.push_back(t); position+=uint64_t(total)*sector;
            c.skip(29);
            if (version!=0x80000004) { c.skip(5); if (c.read(4)==UINT32_MAX) c.skip(78); }
        }
        c.skip(version==0x80000004?12:13);
    }
    // rcheevos' Dreamcast algorithm probes GD-ROM track 3 first. A self-boot
    // CDI can have two DATA sessions: the first contains a bootstrap/dummy
    // IP.BIN, while the actual game lives in the last session. Translate that
    // boot-track request only for our explicit Dreamcast CDI hash operation.
    if (requested == 3 && h.context && h.context->dreamcast_cdi && !tracks.empty()) {
        const auto last_session = tracks.back().session;
        const auto boot = std::find_if(tracks.begin(), tracks.end(), [last_session](const Track& t) {
            return t.session == last_session && !t.audio;
        });
        require(boot != tracks.end(), "CDI boot session has no data track");
        h.track = *boot;
    } else {
        h.track=select_track(tracks,requested);
    }
}
void* RC_CCONV open_track(const char* path, uint32_t requested, const rc_hash_iterator_t* iterator) {
    try {
        auto h=std::make_unique<Handle>(); h->context=hash_context(iterator);
        if (h->cancelled()) return nullptr;
        const auto ext=extension(path);
        if (ext==".chd") open_chd(*h,path,requested);
        else if (ext==".cdi") open_cdi(*h,path,requested);
        else {
            rc_hash_get_default_cdreader(&h->fallback);
            h->fallback_handle=h->fallback.open_track_iterator(path,requested,iterator);
            if (!h->fallback_handle) return nullptr;
        }
        return h.release();
    } catch (const std::exception& e) { set_error(iterator,e.what()); return nullptr; }
}
size_t RC_CCONV read_sector(void* handle, uint32_t sector, void* buffer, size_t bytes) {
    auto& h=*static_cast<Handle*>(handle);
    if (h.cancelled()) return 0;
    if (h.fallback_handle) return h.fallback.read_sector(h.fallback_handle,sector,buffer,bytes);
    if (sector<h.track.start) return 0;
    uint64_t relative=uint64_t(sector)-h.track.start;
    size_t read=0;
    while (bytes && relative<h.track.frames && !h.cancelled()) {
        const size_t amount=std::min(bytes,size_t(2048));
        if (h.chd) {
            const uint8_t* data=nullptr;
            if (!chd_frame(h,h.track.offset+relative,data)) break;
            std::memcpy(static_cast<uint8_t*>(buffer)+read,data+h.track.header,amount);
        } else {
            const uint64_t offset=h.track.offset+relative*h.track.sector_size+h.track.header;
            if (fseeko(h.file,static_cast<off_t>(offset),SEEK_SET)!=0 ||
                std::fread(static_cast<uint8_t*>(buffer)+read,1,amount,h.file)!=amount) {
                if (h.context) { h.context->io_failed=true; h.context->error="Could not read complete CDI sector"; }
                break;
            }
        }
        read+=amount; bytes-=amount; ++relative;
    }
    if (bytes && (read || bytes > 1) && !h.cancelled() && h.context && !h.context->io_failed) {
        // rcheevos can finalize an executable hash after a short read. Do not
        // accept that partial hash when the directory overstates its extent.
        // Its one-byte Dreamcast probe may legitimately target another track.
        h.context->io_failed = true;
        h.context->error = "Disc data extends beyond the selected track";
    }
    return read;
}
void RC_CCONV close_track(void* h) { delete static_cast<Handle*>(h); }
uint32_t RC_CCONV first_sector(void* handle) {
    auto& h=*static_cast<Handle*>(handle);
    return h.fallback_handle?h.fallback.first_track_sector(h.fallback_handle):h.track.start;
}
// rc_hash_merge_callbacks tests the legacy field, even when iterator callbacks
// are used. All modern calls dispatch to open_track_iterator above.
void* RC_CCONV legacy_marker(const char*,uint32_t) { return nullptr; }
void RC_CCONV hash_verbose(const char*,const rc_hash_iterator_t* it) { hash_context(it); }
void RC_CCONV hash_error(const char* message,const rc_hash_iterator_t* it) {
    auto* context = hash_context(it);
    if (!context->io_failed) context->error=message;
}
}
rc_hash_cdreader_t disc_reader() {
    rc_hash_cdreader_t reader={};
    reader.open_track=legacy_marker; reader.open_track_iterator=open_track;
    reader.read_sector=read_sector; reader.close_track=close_track; reader.first_track_sector=first_sector;
    return reader;
}
bool uses_disc_reader(const std::string& path) { const auto ext=extension(path); return ext==".chd" || ext==".cdi"; }
DiscHashResult hash_disc(const std::string& path,uint32_t console,const std::atomic<bool>& cancel) {
    rc_hash_iterator_t iterator={};
    rc_hash_initialize_iterator(&iterator,path.c_str(),nullptr,0);
    HashContext context{};
    context.cancel=&cancel;
    iterator.userdata=&context; iterator.callbacks.cdreader=disc_reader();
    iterator.callbacks.verbose_message=hash_verbose; iterator.callbacks.error_message=hash_error;
    char hash[33]={};
    if (!console && extension(path)==".cdi") console=RC_CONSOLE_DREAMCAST;
    context.dreamcast_cdi = console == RC_CONSOLE_DREAMCAST && extension(path)==".cdi";
    const int ok=console?rc_hash_generate(hash,console,&iterator):rc_hash_iterate(hash,&iterator);
    rc_hash_destroy_iterator(&iterator);
    if (cancel.load()) return {{},"Hash cancelled"};
    if (ok && !context.io_failed) return {hash,{}};
    if (!context.reader_error.empty()) return {{},context.reader_error};
    return {{},context.error.empty()?"Disc hash generation failed":context.error};
}
}}
