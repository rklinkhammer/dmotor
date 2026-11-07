#include "acquisition.hpp"
#include <cassert>
#include <iostream>
using namespace dmotor_monitor;
struct FakeClock : Clock {
    Us t = 1000; unsigned calls = 0, jump_at = 999; Us jump = 0;
    Us now() override { if (calls++ == jump_at) t += jump; return t; }
};
struct FakeBus : Bus {
    FakeClock& clock; unsigned calls = 0, fail_at = 99, delay_at = 99;
    Us delay = 0, normal = 450;
    explicit FakeBus(FakeClock& c) : clock(c) {}
    bool read(unsigned d, std::uint8_t reg, int wait, std::uint16_t& v) override {
        assert(d < 4 && (reg == 6 || reg == 1 || reg == 2));
        assert(wait > 0 && wait <= 2);
        const unsigned i = calls++;
        clock.t += normal + (i == delay_at ? delay : 0);
        v = 123; return i != fail_at;
    }
};
int main() {
    assert(timeout_ms(2000,100) == 0);
    assert(timeout_ms(999,1000) == 0);
    assert(timeout_ms(1999,1000) == 1);
    assert(timeout_ms(2000,1000) == 2);
    assert(timeout_ms(10000,100,10) == 10);
    assert(timeout_ms(9999,100,10) == 0);
    assert(timeout_ms(-1,1000) == 0);
    for (unsigned fail = 0; fail < 12; ++fail) {
        FakeClock c; FakeBus b(c); b.fail_at = fail; Acquisition a(c,b);
        auto r = a.run(0);
        assert(r.status == Status::io && b.calls == fail+1 && a.faulted());
        assert(r.raw[0][0] == 0);
        assert(!acquisition_qualified(r,c.t,a.faulted(),true));
        b.fail_at = 99; c.t = 11000;
        auto recovered = a.run(10000);
        assert(!acquisition_qualified(recovered,c.t,a.faulted(),true));
    }
    for (unsigned at = 0; at < 12; ++at) {
        FakeClock c; FakeBus b(c); b.delay_at=at; b.delay=10000;
        Acquisition a(c,b); auto r=a.run(0);
        assert(r.status==Status::deadline && b.calls==at+1);
        assert(!acquisition_qualified(r,c.t,a.faulted(),true));
    }
    {
        FakeClock c; FakeBus b(c); Acquisition a(c,b); auto r=a.run(0);
        assert(r.status==Status::complete && r.reads==12 && c.t==6400);
        assert(acquisition_qualified(r,c.t,false,true));
        assert(!acquisition_qualified(r,c.t,false,false));
        assert(!acquisition_qualified(r,8000,false,true));
        assert(!acquisition_qualified(r,6399,false,true));
    }
    {
        FakeClock c; FakeBus b(c); Acquisition a(c,b); Config cfg; cfg.tick_hz=100;
        assert(a.run(0,cfg).status==Status::unrepresentable && b.calls==0);
    }
    {
        FakeClock c; c.t=8000; FakeBus b(c); Acquisition a(c,b);
        assert(a.run(0).status==Status::deadline && b.calls==0);
    }
    {
        FakeClock c; FakeBus b(c); b.normal=600; Acquisition a(c,b);
        auto r=a.run(0); assert(r.status==Status::unrepresentable && b.calls==11);
    }
    {
        FakeClock c; FakeBus b(c); b.delay_at=0; b.delay=6550; Acquisition a(c,b);
        assert(a.run(0).status==Status::deadline && b.calls==1); // equality rejects
    }
    {
        FakeClock c; FakeBus b(c); b.normal=-1; Acquisition a(c,b);
        assert(a.run(0).status==Status::deadline);
    }
    // Preemption before first read, between reads, and before final publication.
    for (unsigned call : {1u, 3u, 25u}) {
        FakeClock c; c.jump_at=call; c.jump=10000; FakeBus b(c); Acquisition a(c,b);
        auto r=a.run(0);
        assert(r.status==Status::deadline);
        assert(b.calls==(call==1 ? 0u : call==3 ? 1u : 12u));
    }
    for (unsigned mask=1; mask<=15; ++mask) {
        FakeClock c; FakeBus b(c); Acquisition a(c,b); Config cfg; cfg.population=mask;
        auto r=a.run(0,cfg); unsigned n=0;
        for(unsigned i=0;i<4;++i) if(mask & (1u<<i)) n+=3;
        assert(r.status==Status::complete && r.reads==n);
    }
    {
        FakeClock c; FakeBus b(c); Acquisition a(c,b); Config cfg; cfg.population=0;
        assert(a.run(0,cfg).status==Status::config && b.calls==0);
        assert(a.run(std::numeric_limits<Us>::max()).status==Status::config);
    }
    std::cout << "acquisition deterministic tests passed\n";
}
