#pragma once
#include <array>
#include <atomic>
#include <cstdint>
namespace rr {
// Main-thread producer; asynchronous core readers. Each atomic contains an
// independently coherent joypad or stick value. Readers may cross a frame
// boundary, but never touch frontend state, settings, SDL or menu objects.
class AsyncInputSnapshot {
    struct Port { std::atomic<uint32_t> joy{0}, left{0}, right{0}; };
    std::array<Port,4> ports_{};
    static uint32_t pack(int16_t x,int16_t y) { return uint16_t(x)|(uint32_t(uint16_t(y))<<16); }
public:
    static_assert(std::atomic<uint32_t>::is_always_lock_free,"Input snapshots must be lock-free");
    void publish(unsigned port,uint16_t joy,int16_t lx,int16_t ly,int16_t rx,int16_t ry) {
        if(port>=ports_.size()) return;
        ports_[port].left.store(pack(lx,ly),std::memory_order_relaxed);
        ports_[port].right.store(pack(rx,ry),std::memory_order_relaxed);
        ports_[port].joy.store(joy,std::memory_order_relaxed);
    }
    int16_t joy(unsigned port,unsigned id) const {
        if(port>=ports_.size()) return 0;
        const auto bits=ports_[port].joy.load(std::memory_order_relaxed);
        if(id==256) return static_cast<int16_t>(bits); // RETRO_DEVICE_ID_JOYPAD_MASK
        return id<16 ? ((bits>>id)&1) : 0;
    }
    int16_t analog(unsigned port,unsigned index,unsigned id) const {
        if(port>=ports_.size()||index>1||id>1) return 0;
        const auto bits=(index?ports_[port].right:ports_[port].left).load(std::memory_order_relaxed);
        return static_cast<int16_t>(static_cast<uint16_t>(bits>>(id*16)));
    }
};
}
