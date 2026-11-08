#include "monitor_cycle.hpp"
#include <cassert>
#include <iostream>
using namespace dmotor_monitor;
// Synthetic test values only. NOT motor/board recommended settings.
PolicyConfig cfg(unsigned population=1) {
    PolicyConfig c; c.population=population;
    c.pack_min_mv=8000; c.pack_max_mv=18000; c.pair_low_mv=5000;
    c.pair_recover_mv=5500; c.low_persistence_us=32000;
    c.imbalance_mv={2000,1000,32000};
    for(auto& m:c.motor) {
        m.winding_ma={2000,1800,32000}; m.supply_ma={3000,2700,0};
        m.stall_ma=1500; m.stall_us=64000;
    }
    return c;
}
PolicyInput input(unsigned population=1) {
    PolicyInput i; i.population=i.valid_current=population; i.batteries_valid=true;
    i.full_mv=12000; i.midpoint_mv=6000; i.winding_ma.fill(1000); i.supply_ma.fill(500);
    return i;
}
struct ClockImpl:Clock { Us t=100000; Us now() override { return t; } };
struct BusImpl:Bus {
    ClockImpl& c; Us duration=100; int fail_device=-1; unsigned calls=0;
    explicit BusImpl(ClockImpl& clock):c(clock) {}
    bool read(unsigned device,uint8_t reg,int,uint16_t& value) override {
        c.t+=duration; ++calls;
        value=reg==0xfe?0x5449:reg==0xff?0x2260:reg==0?0x4127:reg==6?8:500;
        return int(device)!=fail_device;
    }
};
struct Inputs:MonitorInputs {
    ClockImpl& c; Us duration=50; unsigned reads=0; int clipped=-1; uint32_t causes=0;
    explicit Inputs(ClockImpl& clock):c(clock) {}
    bool adc(unsigned channel,int& raw) override {
        ++reads; c.t+=duration;
        raw=channel==0?1200:channel==1?600:1000;
        if(int(channel)==clipped) raw=4095;
        return true;
    }
    uint32_t faults() override { return causes; }
};
SensorConfig sensors(unsigned mask) {
    SensorConfig c; c.population=mask; c.configuration.fill(0x4127);
    c.conversion_bound_us.fill(2500); return c;
}
Calibration calibration(unsigned mask) {
    Calibration c; c.board=c.generation=1; c.population=mask;
    c.gain_ppm.fill(1000000); c.gain_ppm[8]=c.gain_ppm[9]=10000000;
    c.checksum=c.crc(); return c;
}
CurrentCalibration current(unsigned mask) {
    CurrentCalibration c; c.board=c.generation=1; c.population=mask;
    c.gain_ppm.fill(1000000); c.checksum=c.crc(); return c;
}
int main() {
    {
        MonitorPolicy p(cfg()); auto in=input(); in.winding_ma[0]=2100; in.supply_ma[0]=0;
        assert(p.evaluate(0,100,in).healthy);
        in.winding_ma[0]=1900; assert(p.evaluate(16000,16100,in).healthy); // Hysteresis holds timer.
        auto r=p.evaluate(32000,32100,in); assert((r.causes&current_fault) && r.motors==1);
        in=input(); auto next=p.evaluate(48000,48100,in);
        assert(!next.healthy && next.monitoring_complete); // Fault latch does not hide healthy monitoring.
        in.valid_current=0; next=p.evaluate(64000,64100,in);
        assert(!next.healthy && !next.monitoring_complete);
    }
    {
        MonitorPolicy p(cfg()); auto in=input(); in.winding_ma[0]=2100;
        p.evaluate(0,100,in); in.winding_ma[0]=1700; p.evaluate(16000,16100,in);
        in.winding_ma[0]=2100; assert(p.evaluate(32000,32100,in).healthy);
        assert(p.evaluate(48000,48100,in).healthy);
        assert(p.evaluate(64000,64100,in).causes&current_fault);
    }
    {
        auto c=cfg(); c.motor[0].use_motion=true; c.motor[0].motion_max_age_us=20000;
        MonitorPolicy p(c); auto in=input(); in.winding_ma[0]=1600;
        in.motion[0]={true,true,false,0};
        for(unsigned i=0;i<4;++i) {
            in.motion[0].observed=Us(i)*16000;
            assert(p.evaluate(Us(i)*16000,Us(i)*16000+100,in).healthy);
        }
        in.motion[0].observed=64000;
        assert(p.evaluate(64000,64100,in).causes&stall_fault);
        MonitorPolicy missing(c); assert(missing.evaluate(0,100,input()).causes&input_fault);
        MonitorPolicy stale(c); in.motion[0].observed=0;
        assert(stale.evaluate(16000,20000,in).causes&input_fault);
        MonitorPolicy moving(c); in.motion[0]={true,true,true,0};
        assert(moving.evaluate(0,100,in).healthy);
    }
    {
        MonitorPolicy p(cfg()); auto in=input(); in.winding_ma[0]=1600;
        for(unsigned i=0;i<4;++i) assert(p.evaluate(Us(i)*16000,Us(i)*16000+100,in).healthy);
        assert(p.evaluate(64000,64100,in).causes&stall_fault); // Explicit current-only proxy.
    }
    for(unsigned mode=0;mode<7;++mode) {
        auto c=cfg(); auto in=input();
        if(mode==0) in.supply_ma[0]=-3000; // Supply direction does not hide magnitude.
        if(mode==1) in.hard_causes=1;
        if(mode==2) in.midpoint_mv=13000;
        if(mode==3) in.winding_ma[0]=std::numeric_limits<double>::quiet_NaN();
        if(mode==4) c.motor[0].winding_ma.trip=0;
        if(mode==5) in.valid_current=0;
        if(mode==6) in.full_mv=19000;
        MonitorPolicy p(c); assert(!p.evaluate(0,100,in).healthy);
    }
    {
        MonitorPolicy p(cfg()); auto in=input(); in.full_mv=9000; in.midpoint_mv=4500;
        assert(p.evaluate(0,100,in).healthy); assert(p.evaluate(16000,16100,in).healthy);
        assert(p.evaluate(32000,32100,in).causes&battery_fault);
    }
    {
        MonitorPolicy p(cfg()); auto in=input(); p.evaluate(0,100,in);
        assert(p.evaluate(32000,32100,in).causes&timing_fault);
        MonitorPolicy q(cfg()); assert(q.evaluate(0,12000,in).causes&timing_fault);
    }
    {
        auto c=current(1); assert(c.valid(1,1)); c.gain_ppm[0]++; assert(!c.valid(1,1));
    }
    {
        ClockImpl clock; BusImpl bus(clock); Inputs inputs(clock);
        MonitorCycle cycle(clock,bus,inputs,sensors(15),calibration(15),current(15),1,cfg(15));
        auto r=cycle.run(100000); assert(r.warming && r.adc_reads==6 && bus.calls==24);
        clock.t=116000; r=cycle.run(116000,{},true,true);
        assert(r.policy.healthy && r.proof.valid_sensors==15 && r.proof.policy);
        assert(r.adc_end==116300 && r.finished==118700);
        SafetyCore core; auto token=core.begin(116000,116000);
        assert(core.finish(token,r.finished,r.proof));
        core.observe(132000); assert(core.permits());
        core.observe(144000); assert(!core.permits()); // Missing next worker.
        clock.t=132000; bus.fail_device=2; r=cycle.run(132000);
        assert(!r.policy.healthy && !r.proof.policy);
        bus.fail_device=-1; clock.t=148000; r=cycle.run(148000);
        assert(!r.policy.healthy); // A subsequent warming cannot recover a fault.
    }
    {
        ClockImpl clock; BusImpl bus(clock); Inputs inputs(clock); bus.duration=570;
        MonitorCycle cycle(clock,bus,inputs,sensors(15),calibration(15),current(15),1,cfg(15));
        auto r=cycle.run(100000); assert(!r.policy.healthy && !r.warming);
        assert(bus.calls<24); // Measured per-read cost cannot fit 24 reads; no budget widening.
    }
    {
        ClockImpl clock; BusImpl bus(clock); Inputs inputs(clock);
        MonitorCycle cycle(clock,bus,inputs,sensors(1),calibration(1),current(1),1,cfg());
        assert(cycle.run(100000).warming);
        clock.t=116000; inputs.causes=1;
        auto r=cycle.run(116000);
        assert(!r.policy.healthy && r.policy.monitoring_complete && r.proof.monitoring_complete);
        SafetyCore core{1}; auto token=core.begin(116000,116000);
        assert(!core.finish(token,r.finished,r.proof));
        assert(core.heartbeat() && !core.permits() && core.faulted());
        clock.t=132000; inputs.causes=0;
        r=cycle.run(132000); token=core.begin(132000,132000);
        assert(r.proof.monitoring_complete && !r.proof.policy);
        assert(!core.finish(token,r.finished,r.proof));
        assert(core.monitoring_live() && !core.heartbeat() && !core.permits());
        clock.t=148000; bus.fail_device=0;
        r=cycle.run(148000); token=core.begin(148000,148000);
        assert(!r.proof.monitoring_complete);
        assert(!core.finish(token,r.finished,r.proof) && !core.monitoring_live());
    }
    for(int mode=0;mode<4;++mode) {
        ClockImpl clock; BusImpl bus(clock); Inputs inputs(clock); auto cal=calibration(1);
        if(mode==0) inputs.clipped=2;
        if(mode==1) inputs.duration=400;
        if(mode==2) cal.checksum^=1;
        if(mode==3) inputs.causes=1;
        MonitorCycle cycle(clock,bus,inputs,sensors(1),cal,current(1),1,cfg());
        auto r=cycle.run(100000); assert(!r.policy.healthy && !r.warming);
    }
    std::cout << "monitor policy/workload PASS: bounded ADC+INA, current/stall, calibration, faults and liveness\n";
}
