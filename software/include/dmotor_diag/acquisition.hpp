#pragma once
#include "uart.hpp"
namespace dmotor_acq_diag {
// Separate diagnostic envelope. Never an operational monitor/recovery frame.
constexpr uint8_t magic = 0xd8;
using dmotor_diag::Packet;
using dmotor_diag::put;
using dmotor_diag::get;
struct Snapshot {
    uint64_t boot=0;
    uint32_t cycle=0, duration=0, age=0, failures=0, skipped=0, expired=0;
    uint8_t status=1, reads=0, address=0, injected=0;
    std::array<uint16_t,12> raw{};
    uint16_t adc0=0, adc1=0;
};
inline Packet request(uint64_t run, uint32_t seq) {
    Packet p; p.kind=1; p.run=run; p.sequence=seq; return p;
}
inline Packet response(const Packet& q, const Snapshot& s) {
    Packet p=q; p.kind=2; p.length=64;
    auto* b=p.payload.data();
    put(b,s.boot,8); put(b+8,s.cycle,4); put(b+12,s.duration,4); put(b+16,s.age,4);
    put(b+20,s.failures,4); put(b+24,s.skipped,4); put(b+28,s.expired,4);
    b[32]=s.status; b[33]=s.reads; b[34]=s.address; b[35]=s.injected;
    for(unsigned i=0;i<12;++i) put(b+36+2*i,s.raw[i],2);
    put(b+60,s.adc0,2); put(b+62,s.adc1,2); return p;
}
inline Snapshot unpack(const Packet& p) {
    Snapshot s; const auto* b=p.payload.data();
    s.boot=get(b,8); s.cycle=static_cast<uint32_t>(get(b+8,4));
    s.duration=static_cast<uint32_t>(get(b+12,4)); s.age=static_cast<uint32_t>(get(b+16,4));
    s.failures=static_cast<uint32_t>(get(b+20,4)); s.skipped=static_cast<uint32_t>(get(b+24,4));
    s.expired=static_cast<uint32_t>(get(b+28,4));
    s.status=b[32]; s.reads=b[33]; s.address=b[34]; s.injected=b[35];
    for(unsigned i=0;i<12;++i) s.raw[i]=static_cast<uint16_t>(get(b+36+2*i,2));
    s.adc0=static_cast<uint16_t>(get(b+60,2)); s.adc1=static_cast<uint16_t>(get(b+62,2));
    return s;
}
// Receiver checks transport recency, NOT INA conversion freshness. Different
// clocks are never subtracted. Age is remote age plus local request round trip.
enum class Rejection { none, envelope, challenge, sequence, boot, cycle, expiry,
    skipped, failure, sample, reads, flags, mode, address, duration, age };
inline const char* reason(Rejection r) {
    switch(r) {
#define R(x) case Rejection::x: return #x
        R(none); R(envelope); R(challenge); R(sequence); R(boot); R(cycle);
        R(expiry); R(skipped); R(failure); R(sample); R(reads); R(flags);
        R(mode); R(address); R(duration); R(age);
#undef R
    }
    return "unknown";
}
class Tracker {
    uint64_t boot_=0; uint32_t cycle_=0;
public:
    Rejection check(const Packet& p, uint64_t run, uint32_t seq,
                    uint64_t roundtrip_us, bool synthetic=false) {
        if(p.kind!=2 || p.length!=64) return Rejection::envelope;
        if(p.run!=run) return Rejection::challenge;
        if(p.sequence!=seq) return Rejection::sequence;
        const auto s=unpack(p);
        if(!s.boot || (boot_ && s.boot!=boot_)) return Rejection::boot;
        if(!s.cycle || s.cycle<=cycle_) return Rejection::cycle;
        if(s.expired) return Rejection::expiry;
        if(s.skipped) return Rejection::skipped;
        if(s.failures) return Rejection::failure;
        if(s.status) return Rejection::sample;
        if(s.reads!=12) return Rejection::reads;
        if((s.injected & 1) || s.injected>3) return Rejection::flags;
        if(bool(s.injected & 2)!=synthetic) return Rejection::mode;
        if(s.address<0x40 || s.address>0x4f) return Rejection::address;
        if(s.duration>=11000) return Rejection::duration;
        if(roundtrip_us>100000 || s.age>50000 || roundtrip_us+s.age>100000)
            return Rejection::age;
        boot_=s.boot; cycle_=s.cycle; return Rejection::none;
    }
    bool accept(const Packet& p, uint64_t run, uint32_t seq, uint64_t elapsed, bool synthetic=false) {
        return check(p,run,seq,elapsed,synthetic)==Rejection::none;
    }
};
// Task-level independent deadline observer. Caller serializes access. No output
// authorization exists: this gate only qualifies a diagnostic transport sample.
struct DeadlineGate {
    uint64_t generation=0, deadline=0;
    bool active=false, expired=false;
    void begin(uint64_t g,uint64_t d) { generation=g; deadline=d; active=true; expired=false; }
    bool check(uint64_t now) {
        if(active && now>=deadline) { expired=true; active=false; return true; }
        return false;
    }
    bool finish(uint64_t g,uint64_t now) {
        check(now);
        if(g!=generation || expired || !active) return false;
        active=false; return true;
    }
};
} // namespace dmotor_acq_diag
