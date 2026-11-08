#include "dmotor_diag/acquisition.hpp"
#include <cassert>
#include <iostream>
using namespace dmotor_acq_diag;
int main() {
    Snapshot s; s.boot=123; s.cycle=1; s.duration=7697; s.age=1000;
    s.status=0; s.reads=12; s.address=0x44; s.raw[2]=0x7e7d;
    auto q=request(456,1); auto p=response(q,s);
    std::array<uint8_t,dmotor_diag::max_wire> bytes{};
    auto n=dmotor_diag::encode(p,bytes,magic);
    dmotor_diag::Decoder decoder(magic),old_decoder;
    Packet out;
    unsigned packets=0,old_packets=0;
    for(size_t i=0;i<n;++i) {
        if(decoder.push(bytes[i],out)==dmotor_diag::Decode::packet) ++packets;
        if(old_decoder.push(bytes[i],out)==dmotor_diag::Decode::packet) ++old_packets;
    }
    assert(packets==1 && old_packets==0 && unpack(out).raw[2]==0x7e7d);
    Tracker t; assert(t.accept(p,456,1,10000));
    assert(!t.accept(p,456,1,10000)); // replayed cycle
    for(unsigned fault=0;fault<10;++fault) {
        auto bad=s;
        switch(fault) {
        case 0: bad.status=1; break;
        case 1: bad.reads=9; break;
        case 2: bad.age=50001; break;
        case 3: bad.duration=11000; break;
        case 4: bad.failures=1; break;
        case 5: bad.skipped=1; break;
        case 6: bad.expired=1; break;
        case 7: bad.injected=1; break;
        case 8: bad.boot=0; break;
        case 9: bad.cycle=0; break;
        }
        Tracker fresh; assert(!fresh.accept(response(q,bad),456,1,10000));
    }
    s.cycle=2; s.boot=124; assert(!t.accept(response(q,s),456,1,10000));
    Tracker fresh; assert(!fresh.accept(p,457,1,0)); assert(!fresh.accept(p,456,2,0));
    assert(!fresh.accept(p,456,1,100001));
    auto simulated=unpack(p); simulated.injected=2;
    Tracker real_mode, synthetic_mode;
    assert(!real_mode.accept(response(q,simulated),456,1,10000));
    assert(synthetic_mode.accept(response(q,simulated),456,1,10000,true));
    Tracker wrong_mode; assert(!wrong_mode.accept(p,456,1,10000,true));
    DeadlineGate gate; gate.begin(1,12000);
    assert(!gate.check(11999)); assert(gate.check(12000));
    assert(!gate.finish(1,12001)); // late worker cannot undo expiry
    gate.begin(2,28000); assert(!gate.finish(1,20000));
    assert(!gate.check(20000)); assert(gate.finish(2,27999));
    assert(!gate.check(28000));
    gate.begin(3,44000); assert(!gate.finish(3,44000));
    std::cout << "acquisition link codec, replay, stale, restart and deadline tests passed\n";
}
