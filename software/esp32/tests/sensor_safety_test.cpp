#include "physical_acquisition.hpp"
#include "health_capture.hpp"
#include "release_wait.hpp"
#include "safety_core.hpp"
#include <cassert>
#include <iostream>
using namespace dmotor_monitor;
SensorConfig config() {
    SensorConfig c; c.population=1; c.address[0]=0x44;
    c.configuration[0]=0x4127; c.conversion_bound_us[0]=2500; return c;
}
PhysicalSample sample(Us start=20000) {
    PhysicalSample s; s.transport=true; s.manufacturer=0x5449; s.die=0x2260;
    s.configuration=0x4127; s.mask=8; s.shunt=65533; s.bus=1;
    s.mask_started=start; s.mask_finished=start+100; s.finished=start+1000; return s;
}
SafetyProof proof(Us oldest=0) {
    SafetyProof p; p.population=p.valid_sensors=1;
    p.monitoring_complete=true;
    p.battery0=p.battery1=p.calibration=p.policy=p.recovery=p.physical_arm=true;
    p.oldest_conversion=oldest; return p;
}
struct TestClock : Clock { Us t=100000; Us now() override { return t; } };
struct TestBus : Bus {
    TestClock& c; int fail_reg=-1; uint16_t cfg=0x4127;
    explicit TestBus(TestClock& clock):c(clock) {}
    bool read(unsigned,uint8_t reg,int,uint16_t& v) override {
        c.t+=570;
        v=reg==0xfe?0x5449:reg==0xff?0x2260:reg==0?cfg:reg==6?8:1;
        return reg!=fail_reg;
    }
};
int main() {
    {
        HealthCapture<3> c;
        HealthRecord r; r.health=Health::warming; r.complete=true; c.add(r);
        for(unsigned i=1;i<10000;++i) {
            r.cycle=i; r.health=Health::good; r.age=20000; r.duration=3500; c.add(r);
        }
        assert(c.cycles==10000 && c.health_counts[0]==9999 && !c.has_bad);
        assert(c.retained()==3 && c.at(0).cycle==9997 && c.at(2).cycle==9999);
        r.cycle=10000; r.health=Health::io; r.complete=false; r.bus_error=263;
        r.error_reg=0xfe; r.lateness=1000; r.duration=11000; r.age=-1; c.add(r);
        assert(c.bus_errors==1 && c.guarded==0 && c.late_releases==1 && c.deadline_misses==1);
        r.cycle=10001; r.bus_error=0; c.add(r);
        assert(c.guarded==1 && c.first_bad.cycle==10000 && c.first_bad.error_reg==0xfe);
        assert(c.max_age==20000 && c.max_duration==11000 && c.max_lateness==1000);
    }

    {
        TestClock clock;
        const Us base=clock.t;
        for(unsigned cycle=1;cycle<100;++cycle) {
            const Us release=base+static_cast<Us>(cycle)*monitor_period_us;
            clock.t=release-539; // Reproduce measured FreeRTOS tick phase.
            unsigned calls=0;
            assert(wait_for_release(clock,release,[&](Us remaining) {
                ++calls;
                clock.t+=calls==1 ? 100 : remaining; // Deliberately early first wake.
            }));
            assert(clock.t==release && calls==2);
        }
        clock.t=base;
        assert(!wait_for_release(clock,base+16000,[&](Us) { --clock.t; }));
        clock.t=base+16001;
        assert(wait_for_release(clock,base+16000,[](Us) { assert(false); }));
        assert(clock.t==base+16001); // Late release is not hidden or rebased.
    }

    auto c=config(); assert(c.valid());
    c.population=3; c.address[1]=0x44; c.configuration[1]=0x4127; c.conversion_bound_us[1]=2500;
    assert(!c.valid()); // No alias population.
    c=config(); c.conversion_bound_us[0]=100; assert(!c.valid()); c=config();
    Freshness f(c); auto s=sample();
    assert(f.observe(0,s,21000).health==Health::warming);
    s=sample(36000); auto m=f.observe(0,s,37000);
    assert(m.health==Health::good && m.shunt_raw==-3 && m.oldest_possible_us==17500);
    s=sample(52000); s.mask=0;
    assert(f.observe(0,s,53000).health==Health::not_ready); // Readable but no conversion.
    s=sample(68000); s.transport=false;
    assert(f.observe(0,s,69000).health==Health::io);
    s=sample(84000); assert(f.observe(0,s,85000).health==Health::warming);
    s=sample(110000); assert(f.observe(0,s,111000).health==Health::stale);
    s=sample(126000); s.manufacturer=0;
    assert(f.observe(0,s,127000).health==Health::identity);
    s=sample(142000); s.configuration^=1;
    assert(f.observe(0,s,143000).health==Health::config);
    s=sample(158000); assert(f.observe(0,s,159000).health==Health::warming);
    s=sample(174000); s.mask=12;
    assert(f.observe(0,s,175000).health==Health::overflow);
    s=sample(160000); assert(f.observe(0,s,161000).health==Health::clock);
    assert(f.observe(1,s,161000).health==Health::config);
    Freshness boundary(c); s=sample(30000); boundary.observe(0,s,31000);
    s=sample(51000); assert(boundary.observe(0,s,52499).health==Health::good);
    // Exact 25 ms is rejected by final output qualification regardless of snapshot.
    Calibration cal; cal.board=2; cal.generation=1; cal.population=1;
    cal.gain_ppm.fill(1000000); cal.checksum=cal.crc(); double v=99;
    assert(cal.convert(0,-3,2,1,v) && v==-3);
    assert(!cal.convert(1,1,2,1,v) && v==0);
    assert(!cal.valid(3,1) && !cal.valid(2,3));
    cal.gain_ppm[8]++; assert(!cal.valid(2,1));
    cal.gain_ppm[8]=0; cal.checksum=cal.crc(); assert(!cal.valid(2,1));
    TestClock clock; TestBus bus(clock); PhysicalAcquisition acq(clock,bus,c);
    auto r=acq.run(clock.t); assert(r.complete && r.valid==0);
    clock.t=116000; r=acq.run(clock.t); assert(r.complete && r.valid==1);
    clock.t=132000; bus.fail_reg=2; r=acq.run(clock.t); assert(!r.complete && !r.valid);
    clock.t=148000; bus.fail_reg=-1; r=acq.run(clock.t); assert(r.complete && !r.valid);
    clock.t=164000; bus.cfg=0; r=acq.run(clock.t); assert(r.complete && !r.valid);
    {
        SafetyCore core{1}; auto t=core.begin(0,0);
        assert(core.finish(t,1000,proof()) && core.permits() && core.heartbeat());
        t=core.begin(16000,16000);
        assert(core.finish(t,17000,proof(16000)) && !core.heartbeat());
        core.observe(40999); assert(core.permits());
        core.observe(41000); assert(core.permits()); // D10: sample age is not a continuous permit timer.
        core.observe(44000); assert(core.faulted() && !core.permits()); // Next work deadline wins.
    }
    {
        SafetyCore core{1}; auto t=core.begin(100000,100000);
        assert(core.finish(t,104000,proof(83760))); // 20,240 us old at commit.
        core.observe(109000); assert(core.permits()); // Old sample age exceeds 25 ms now.
        t=core.begin(116000,116000);
        assert(core.finish(t,120000,proof(99760))); // New independently qualified sample.
        core.observe(144000); assert(core.faulted()); // Missing next completion still inhibits.
    }
    {
        SafetyCore core{1}; auto p=proof(); p.recovery=p.physical_arm=false;
        auto t=core.begin(0,0); assert(!core.finish(t,1000,p));
        assert(core.monitoring_live() && core.heartbeat() && !core.permits());
        t=core.begin(16000,16000); p.oldest_conversion=16000; p.policy=false;
        assert(!core.finish(t,17000,p) && core.faulted());
        assert(core.monitoring_live() && !core.heartbeat() && !core.permits());
        t=core.begin(32000,32000); p=proof(32000);
        assert(!core.finish(t,33000,p)); // Latched fault prevents permission, not monitoring.
        assert(core.heartbeat() && core.monitoring_live());
        core.observe(60000); assert(!core.heartbeat() && !core.monitoring_live());
        t=core.begin(64000,64000); p=proof(64000);
        assert(!core.finish(t,65000,p) && core.heartbeat() && core.faulted());
        t=core.begin(80000,80000); p=proof(80000); p.monitoring_complete=false;
        assert(!core.finish(t,81000,p) && !core.monitoring_live());
    }
    {
        SafetyCore core{1}; auto p=proof(); p.recovery=false;
        auto t=core.begin(0,0); assert(!core.finish(t,100,p) && core.heartbeat());
        assert(!core.finish(t,200,p) && core.heartbeat()); // Duplicate completion cannot toggle.
        t=core.begin(0,300); assert(!core.finish(t,400,p) && !core.monitoring_live());
        t=core.begin(16000,16000); p=proof(16000);
        assert(!core.finish(t,16100,p) && core.heartbeat()); // Latch still holds permits low.
        t=core.begin(32000,32000); p=proof(32000);
        assert(!core.finish(t,32100,p) && !core.heartbeat() && core.monitoring_live());
        core.observe(60000); assert(!core.monitoring_live()); // Low waveform phase still expires.
    }
    for(bool fault_first:{false,true}) {
        SafetyCore core{1}; auto t=core.begin(0,0);
        if(fault_first) core.trip();
        bool committed=core.finish(t,1000,proof());
        if(!fault_first) { assert(committed); core.trip(); }
        else assert(!committed);
        assert(!core.permits() && !core.heartbeat()); // Both serialized race orders.
        core.begin(16000,16000); assert(!core.finish(t,17000,proof(16000)));
    }
    {
        SafetyCore core{1}; auto t=core.begin(0,0); core.observe(12000);
        assert(!core.finish(t,12001,proof()) && core.faulted()); // Blocked worker.
    }
    {
        SafetyCore core{1}; auto t=core.begin(0,0);
        assert(core.finish(t,11999,proof()));
        core.begin(32000,32000); assert(core.faulted()); // Missed cycle.
    }
    {
        SafetyCore core{15}; auto t=core.begin(0,0);
        assert(!core.finish(t,1000,proof())); // A worker cannot shrink installed population.
    }
    for(unsigned missing=0;missing<8;++missing) {
        SafetyCore core{1}; auto t=core.begin(0,0); auto p=proof();
        switch(missing) {
            case 0:p.valid_sensors=0;break; case 1:p.battery0=false;break;
            case 2:p.battery1=false;break; case 3:p.calibration=false;break;
            case 4:p.policy=false;break; case 5:p.recovery=false;break;
            case 6:p.physical_arm=false;break; case 7:p.oldest_conversion=2000;break;
        }
        assert(!core.finish(t,1000,p) && !core.permits());
        if(missing==5 || missing==6) assert(!core.faulted());
    }
    {
        SafetyCore core{1}; auto t=core.begin(16000,16000);
        assert(!core.finish(t,25000,proof())); // Exact age boundary.
    }
    std::cout << "physical sensor health and serialized safety PASS\n";
}
