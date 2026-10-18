#include "dmotor_sw/runtime.hpp"
#include "dmotor_sw/version.hpp"
#include "dmotor/controller_registers.hpp"
#include <map>
#include <vector>
#include <iostream>
#include <cstdlib>
#define CHECK(x) do { if(!(x)) { std::cerr<<__LINE__<<": " #x "\n";std::exit(1); } } while(false)
using namespace dmotor_sw;
namespace reg=dmotor::controller_reg;
namespace b=dmotor::batch_reg;
struct Fake : Platform {
    std::map<uint32_t,uint32_t> words;
    std::vector<std::pair<uint32_t,uint32_t>> writes;
    uint64_t time=0;bool source=true,reset=true,fail=false,hold_ready=false,halt_failure=false;
    unsigned halt_count=0;
    Fake() { words[reg::IDENTITY]=reg::PROFILE_ID;words[reg::STATUS]=1U<<18;words[b::IDENTITY]=b::PROFILE_ID;words[b::SCHEMA]=1;words[b::MOTOR_COUNT]=4; }
    uint32_t read32(uint32_t a) override { if(fail) throw IoError("bus");return words[a]; }
    void write32(uint32_t a,uint32_t v) override {
        if(fail) throw IoError("bus");
        writes.emplace_back(a,v);
        if(a==reg::SESSION) { CHECK(reset && !(words[reg::STATUS]&15));words[b::SESSION]=v; }
        if(a==reg::CORE_RESET) { reset=v!=0;if(reset) words[reg::STATUS]|=1U<<18;else if(!source) words[reg::STATUS]&=~(1U<<18); }
        if(a==reg::ENABLE) { CHECK(!reset && !source);words[reg::STATUS]|=v;if(!hold_ready) words[reg::STATUS]|=v<<8; }
        if(a==reg::DISABLE) words[reg::STATUS]&=~(v|(v<<8));
        if(a==reg::STOP) { words[reg::STATUS]|=v<<12;words[reg::STATUS]&=~(v<<8);words[b::PENDING_MASK]=0; }
        if(a==b::COMMIT) {
            CHECK(v==1 && !source && !reset);
            words[b::PUBLISHED_SEQUENCE]=words[b::STAGE_SEQUENCE];words[b::APPLIED_SEQUENCE]=words[b::STAGE_SEQUENCE];
            words[b::APPLIED_MASK]=words[b::STAGE_MASK];words[b::PENDING_MASK]=0;
        }
        if(a==reg::ACKNOWLEDGE && !source) words[reg::STATUS]&=~((1U<<16)|(1U<<17));
        words[a]=v;
    }
    void fence() override { if(fail) throw IoError("fence"); }
    void halt(bool asserted) override { if(halt_failure) throw IoError("HALT confirmation unavailable");source=asserted;if(asserted) { ++halt_count;words[reg::STATUS]&=~15U;words[reg::STATUS]|=1U<<18; } }
    uint64_t now_us() const override { return time; }
    void sleep_us(uint32_t us) override { time+=us; }
};
std::array<MotorConfiguration,4> config() {
    std::array<MotorConfiguration,4> c{};
    for(auto& m:c) m={7,400000,1000000,2000000,1560,1,1,100,2,655,65536,32768,16384,1000000,2000000,1000000,8192};
    return c;
}
template<class F> bool rejects(F f) { try { f(); } catch(const std::exception&) { return true; }return false; }
int main(int argc,char** argv) {
    if(startup_version("dmotor-runtime-tests",argc,argv)) return 0;
    {
        Fake io;Runtime r(io);CHECK(io.source);
        CHECK(rejects([&]{r.enable(1);}));r.prepare(config(),15);CHECK(!io.source && r.session()==1);
        r.enable(3);r.tick();CHECK(io.words[reg::REFRESH_BASE]==1 && io.words[reg::REFRESH_BASE+4]==1);
        r.velocity(3,{{400000,-400000,0,0}});r.tick();CHECK(io.words[b::STAGE_MASK]==3 && r.applied_sequence()==1);
        CHECK(rejects([&]{r.velocity(1,{{400001,0,0,0}});}));
        // Different pending masks are never overwritten, even when disjoint.
        io.words[b::PENDING_MASK]=1;r.velocity(2,{{0,1000,0,0}});r.tick();CHECK(io.words[b::PUBLISHED_SEQUENCE]==1);
        io.words[b::PENDING_MASK]=0;io.time+=10000;r.tick();CHECK(io.words[b::PUBLISHED_SEQUENCE]==2);
        r.stop(1);CHECK(io.words[reg::STOP]==1);CHECK(rejects([&]{r.velocity(1,{{1000,0,0,0}});}));
        // STOP remains authorized/refreshed and must not fail readiness later.
        for(int i=0;i<30;++i) { io.time+=20000;r.heartbeat();r.tick(); }
        CHECK(!io.source);r.disable(2);CHECK(rejects([&]{r.enable(2);}));
        r.halt();CHECK(io.source);r.prepare(config(),15);CHECK(r.session()==2 && !(io.words[reg::STATUS]&15));
    }
    {
        Fake io;Runtime r(io);r.prepare(config(),1);r.enable(1);r.tick();
        io.time+=500000;CHECK(rejects([&]{r.tick();}));CHECK(io.source);
        r.heartbeat();r.tick();CHECK(io.source); // no automatic recovery
    }
    {
        Fake io;Runtime r(io);r.prepare(config(),1);r.enable(1);io.words[reg::STATUS]|=1U<<16;
        CHECK(rejects([&]{r.tick();}));CHECK(io.source);
    }
    {
        Fake io;Runtime r(io);r.prepare(config(),1);r.enable(1);io.fail=true;
        CHECK(rejects([&]{r.tick();}));CHECK(io.source);
    }
    {
        Fake io;io.hold_ready=true;Runtime r(io);r.prepare(config(),1);r.enable(1);
        for(int i=0;i<24;++i) { io.time+=20000;r.heartbeat();r.tick(); }
        io.time+=20000;r.heartbeat();CHECK(rejects([&]{r.tick();}));CHECK(io.source);
    }
    {
        Fake io;io.words[reg::SESSION]=UINT32_MAX;Runtime r(io);
        CHECK(rejects([&]{r.prepare(config(),1);}));CHECK(io.source);
    }
    {
        Fake io;Runtime r(io);r.prepare(config(),1);r.enable(1);
        io.halt_failure=true;CHECK(rejects([&]{r.halt();}));
        CHECK(rejects([&]{r.enable(1);})); // Never report/reuse unconfirmed inhibition.
        io.halt_failure=false;r.halt();CHECK(io.source);
    }
    {
        Fake io;Runtime r(io);r.prepare(config(),1);r.enable(1);
        r.velocity(1,{{1000,0,0,0}});r.tick();
        CHECK(rejects([&]{r.inject_timeout(TimeoutInjection::target);}));
    }
    for(auto injection:{TimeoutInjection::target,TimeoutInjection::watchdog}) {
        Fake io;RuntimePolicy policy;policy.commissioning=true;Runtime r(io,policy);
        CHECK(rejects([&]{r.inject_timeout(injection);}));
        r.prepare(config(),1);r.enable(1);r.velocity(1,{{1000,0,0,0}});r.tick();
        const auto refresh=io.words[reg::REFRESH_BASE];
        const auto published=io.words[b::PUBLISHED_SEQUENCE];
        r.inject_timeout(injection);
        CHECK(rejects([&]{r.inject_timeout(injection);}));
        for(int i=0;i<5;++i) { io.time+=20000;r.heartbeat();r.tick(); }
        CHECK((io.words[reg::REFRESH_BASE]==refresh)==(injection==TimeoutInjection::watchdog));
        CHECK((io.words[b::PUBLISHED_SEQUENCE]==published)==(injection==TimeoutInjection::target));
        io.time+=20000;CHECK(rejects([&]{r.tick();}));CHECK(io.source);
        // A fake without FPGA timers must hit the host fallback; never auto-resume.
        r.tick();CHECK(io.source);r.prepare(config(),1);r.enable(1);
        r.velocity(1,{{1000,0,0,0}});r.tick();CHECK(!io.source);
        r.inject_timeout(injection);io.words[reg::STATUS]|=1U<<16;
        CHECK(rejects([&]{r.tick();}));CHECK(io.source);
    }
    std::cout<<"PASS: lifecycle, 400RPM limits, atomic masks, STOP/rearm, watchdog refresh, lease/fault/access loss and readiness deadlines\n";
}
