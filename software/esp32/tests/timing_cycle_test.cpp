#include "timing_cycle.hpp"
#include <cassert>
#include <iostream>
using namespace dmotor_monitor;
struct FakeClock : Clock {
    Us t=0; unsigned calls=0, jump_at=999; Us jump=10000;
    Us now() override { if(calls++==jump_at) t+=jump; return t; }
};
struct FakeAdc : dmotor_timing::Adc {
    FakeClock& c; unsigned count=0, fail_at=999; Us delay=20;
    explicit FakeAdc(FakeClock& clock): c(clock) {}
    bool read(unsigned channel, int& raw) override {
        assert(channel<6); raw=2000; c.t+=delay; return count++!=fail_at;
    }
};
struct FakeBus : Bus {
    FakeClock& c; unsigned count=0, fail_at=999, delay_at=999;
    Us delay=450, extra=0; unsigned seen_slots=0;
    explicit FakeBus(FakeClock& clock): c(clock) {}
    bool read(unsigned d, std::uint8_t reg, int wait, std::uint16_t& value) override {
        assert(d==count/3); assert(reg==(count%3==0 ? 6 : count%3==1 ? 1 : 2));
        assert(wait>0 && wait<=2); seen_slots|=1u<<d;
        c.t+=delay+(count==delay_at ? extra : 0); value=42;
        return count++!=fail_at;
    }
};
int main() {
    {
        FakeClock c; FakeAdc adc(c); FakeBus bus(c);
        auto r=dmotor_timing::capture(c,bus,adc,0,1000,1,false,false);
        assert(r.adc_complete && r.adc_reads==2 && adc.count==2);
        assert(r.ina.status==Status::complete && bus.count==12 && bus.seen_slots==15);
        assert(r.adc_end==40 && r.ended==5440 && r.ina.deadline==10040);
    }
    {
        FakeClock c; FakeAdc adc(c); FakeBus bus(c);
        auto r=dmotor_timing::capture(c,bus,adc,0,1000,2,true,false);
        assert(r.adc_reads==12 && r.adc_end==240 && bus.count==12);
    }
    // Replay the supplied successful read durations, extending the three missing
    // reads with the largest observed duration. This validates budget arithmetic,
    // not hardware completion or RTOS tick phase.
    for (bool raw : {false,true}) {
        struct MeasuredBus : Bus {
            FakeClock& c; unsigned count=0;
            explicit MeasuredBus(FakeClock& clock):c(clock) {}
            bool read(unsigned, std::uint8_t, int wait, std::uint16_t& value) override {
                const Us duration[]={689,585,571,570,571,572,571,570,571,689,689,689};
                assert(wait==2 && count<12);
                c.t+=duration[count++]; value=8; return true;
            }
        };
        FakeClock c; FakeAdc adc(c); MeasuredBus bus(c);
        auto r=dmotor_timing::capture(c,bus,adc,0,1000,1,false,raw);
        assert(r.ina.status==Status::complete && bus.count==12);
        assert(r.ended < monitor_work_us);
    }
    // Exclusive INA boundary must abort even in the raw-timeout profile.
    for (bool raw : {false,true}) {
        for (Us offset : {-1,0,1}) {
            FakeClock c; FakeAdc adc(c); FakeBus bus(c);
            bus.delay_at=11; bus.extra=ina_budget_us-12*bus.delay+offset;
            auto r=dmotor_timing::capture(c,bus,adc,0,1000,1,false,raw);
            assert(bus.count==12);
            assert(r.ina.status==(offset<0 ? Status::complete : Status::deadline));
            if (offset>=0) assert(r.ina.raw[0][0]==0);
        }
        FakeClock c; FakeAdc adc(c); FakeBus bus(c);
        const Us release=std::numeric_limits<Us>::max()-monitor_period_us+1;
        auto r=dmotor_timing::capture(c,bus,adc,release,1000,1,false,raw);
        assert(r.ina.status==Status::config && adc.count==0 && bus.count==0);
    }
    for(unsigned fail=0;fail<2;++fail) {
        FakeClock c; FakeAdc adc(c); FakeBus bus(c); adc.fail_at=fail;
        auto r=dmotor_timing::capture(c,bus,adc,0,1000,1,false,false);
        assert(!r.adc_complete && r.ina.status==Status::io && bus.count==0);
    }
    {
        FakeClock c; FakeAdc adc(c); FakeBus bus(c); adc.delay=500;
        auto r=dmotor_timing::capture(c,bus,adc,0,1000,1,false,false);
        assert(!r.adc_complete && r.ina.status==Status::deadline && bus.count==0);
    }
    {
        FakeClock c; c.t=1000; FakeAdc adc(c); FakeBus bus(c);
        auto r=dmotor_timing::capture(c,bus,adc,0,1000,1,false,false);
        assert(r.ina.status==Status::deadline && adc.count==0 && bus.count==0);
    }
    for(bool raw : {false,true}) {
        for(unsigned fail=0;fail<12;++fail) {
            FakeClock c; FakeAdc adc(c); FakeBus bus(c); bus.fail_at=fail;
            auto r=dmotor_timing::capture(c,bus,adc,0,1000,1,false,raw);
            assert(r.ina.status==Status::io && bus.count==fail+1 && r.ina.raw[0][0]==0);
        }
        for(unsigned late=0;late<12;++late) {
            FakeClock c; FakeAdc adc(c); FakeBus bus(c); bus.delay_at=late; bus.extra=10000;
            auto r=dmotor_timing::capture(c,bus,adc,0,1000,1,false,raw);
            assert(r.ina.status==Status::deadline && bus.count==late+1 && r.ina.raw[0][0]==0);
        }
    }
    {
        FakeClock c; FakeAdc adc(c); FakeBus bus(c);
        auto r=dmotor_timing::capture(c,bus,adc,0,100,1,false,false);
        assert(r.adc_complete && r.ina.status==Status::unrepresentable && bus.count==0);
    }
    {
        FakeClock c; FakeAdc adc(c); FakeBus bus(c);
        auto r=dmotor_timing::capture(c,bus,adc,0,100,1,false,true);
        assert(r.ina.status==Status::complete && bus.count==12); // fake accepts raw timeout
    }
    for(unsigned samples : {0u,17u}) {
        FakeClock c; FakeAdc adc(c); FakeBus bus(c);
        auto r=dmotor_timing::capture(c,bus,adc,0,1000,samples,false,false);
        assert(r.ina.status==Status::config && adc.count==0);
    }
    {
        FakeClock c; c.jump_at=3; FakeAdc adc(c); FakeBus bus(c);
        auto r=dmotor_timing::capture(c,bus,adc,0,1000,1,false,false);
        assert(r.ina.status==Status::deadline && adc.count==1 && bus.count==0);
    }
    std::cout << "INA plus battery ADC timing cycle tests passed\n";
}
