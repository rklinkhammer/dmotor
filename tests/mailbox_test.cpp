#include "dmotor/mailbox.hpp"
#include "dmotor/timing.hpp"
#include <cstdlib>
#include <iostream>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while(false)
static uint32_t config[20]={1,200000,1000000,1000000,10296,5,1,100,2,655,65536,
    65536,65536,1000000,dmotor::timing::fabric_hz/10,dmotor::timing::fabric_hz*3/50,128,0,0,0};
static uint32_t req[4]={},reply[16]={};
static uint64_t now=0,count=0,edge=0;
static uint32_t epoch=1,direction=0;
static bool auth=true,negative=false;
static dmotor::Controller oracle;
static unsigned compared=0;
static void service(bool tick=false,bool stop=false,bool fault=false,bool reset=false) {
    dmotor_mailbox(reset,req,tick,auth,stop,fault,epoch,now,count,edge,direction,0,
                   negative,config,reply);
}
static void poll() {req[0]=req[1]=req[2]=req[3]=0;service();}
static void step(uint32_t seq=0,int32_t speed=0,bool stop=false,bool fault=false,bool event=false,bool keep_request=false) {
    now+=dmotor::timing::control_tick_cycles;
    if(event){++count;edge=now;}
    if(!keep_request) req[0]=req[1]=req[2]=req[3]=0;
    service(true,stop,fault);
    dmotor::Descriptor d{};d[0]=1;d[2]=seq;d[3]=epoch;d[4]=config[0];d[5]=static_cast<uint32_t>(speed);
    auto r=oracle.step(false,auth,fault,now,{count,edge,direction,0,negative},0,epoch,
                       seq!=0,d,true,stop,dmotor::decode_controller_config(config));
    CHECK(dmotor::signed_word(reply[4])==r.motion.duty_q16);
    CHECK(dmotor::signed_word(reply[5])==r.motion.ramp_mrpm);
    CHECK(dmotor::signed_word(reply[6])==r.velocity.mrpm);
    CHECK((reply[7]&15U)==((r.velocity.valid?1U:0U)|(r.velocity.timed_out?2U:0U)|
                           (r.acquiring?4U:0U)|(r.warming_up?8U:0U)));
    CHECK((reply[8]&0x7fffffffU)==r.faults);
    CHECK((uint64_t{reply[11]}<<32|reply[10])==r.velocity.age);
    ++compared;
}
static void reset() {
    now=count=edge=0;direction=0;negative=false;auth=true;
    poll();service(false,false,false,true);
    oracle=dmotor::Controller{};
    oracle.step(true,false,false,now,{0,0,0,0,false},0,epoch,false,{},true,false,
                dmotor::decode_controller_config(config));
}
static void stage(unsigned field,uint32_t value) {
    req[0]=1;req[1]=field;req[2]=value;req[3]=0;service();CHECK(reply[0]);
}
static void frame(uint32_t seq,int32_t speed,uint32_t session=1,uint32_t generation=1) {
    stage(0,session);stage(1,generation);stage(2,seq);stage(3,static_cast<uint32_t>(speed));
}
static void commit(bool expected=true) {
    req[0]=2;req[1]=req[2]=req[3]=0;service();CHECK(bool(reply[0])==expected);
}
static void warm() {for(unsigned i=0;i<110;++i)step();CHECK(reply[7]&1);}
int main() {
    reset();warm();
    // Partial staging must never change drive, even across a control tick.
    stage(0,1);stage(1,1);step();commit(false);CHECK(!reply[1]);
    frame(1,10000);commit();CHECK(reply[1]&&reply[2]==0);
    // Pending is immutable while next staging is incomplete.
    stage(0,1);stage(1,1);stage(2,2);step(1,10000);CHECK(reply[2]==1);
    stage(3,static_cast<uint32_t>(-10000));commit();
    // Latest complete frame supersedes only the unapplied pending frame.
    frame(3,15000);commit();CHECK(reply[3]==3);step(3,15000);CHECK(reply[2]==3);
    for(unsigned i=0;i<40;++i)step(0,0,false,false,true);
    frame(3,0);commit(false); // Replay.
    frame(2,0);commit(false); // Reordering.
    frame(4,0,2);commit(false); // Wrong session.
    frame(4,0,1,2);commit(false); // Wrong configuration.
    frame(4,200001);commit(false);
    frame(4,INT32_MIN);commit(false);
    frame(4,-200000);commit();step(4,-200000);
    for(unsigned i=0;i<40;++i)step(0,0,false,false,true);
    // Out-of-band stop invalidates partial staging and pending setpoint.
    frame(5,10000);commit();poll();service(false,true);CHECK(!reply[1]);
    step(0,0,true);
    req[0]=2;req[1]=req[2]=req[3]=0;service();CHECK(!reply[0]);
    for(unsigned i=0;i<160;++i)step();
    CHECK(reply[4]==0&&!(reply[7]&16));
    frame(6,1000);commit();auth=false;poll();CHECK(!reply[1]&&reply[4]==0);
    step();auth=true;step();CHECK(reply[2]==4); // No resurrection of sequence 6.
    // A captured fault in a bus-only service inhibits output immediately.
    frame(7,1000);commit();step(7,1000);poll();service(false,false,true);
    CHECK(reply[4]==0&&!reply[1]);step(0,0,false,true);CHECK(reply[8]);
    req[0]=2;req[1]=req[2]=req[3]=0;service();CHECK(!reply[0]);
    // New trusted session after reset; stale software token remains rejected.
    ++epoch;reset();warm();frame(1,1000,1);commit(false);frame(1,1000,2);commit();step(1,1000);
    // Unsigned sequence wrap is not accepted in the same session.
    frame(UINT32_MAX,1000,2);commit();step(UINT32_MAX,1000);
    frame(1,0,2);commit(false);
    // Publication waits through initial feedback qualification.
    ++epoch;reset();frame(1,1000,3);commit();step(1,1000);CHECK(reply[1]&&reply[2]==0);
    for(unsigned i=0;i<105;++i) {
        // Oracle receives the same pending command until it is accepted.
        const bool pending=reply[1]!=0;
        step(pending?1U:0U,1000);
    }
    CHECK(reply[2]==1&&!reply[1]);
    // Commit and tick may coincide; stop must win a coincident commit.
    frame(2,2000,3);req[0]=2;req[1]=req[2]=req[3]=0;
    step(2,2000,false,false,false,true);CHECK(reply[0]&&reply[2]==2);
    frame(3,3000,3);req[0]=2;req[1]=req[2]=req[3]=0;
    step(0,0,true,false,false,true);CHECK(!reply[0]&&!reply[1]&&reply[2]==2);
    for(unsigned i=0;i<120;++i)step();
    frame(4,-1000,3);commit();step(4,-1000);
    ++direction;negative=true;
    for(unsigned i=0;i<40;++i)step(0,0,false,false,true);
    std::cout<<"mailbox: "<<compared<<" control snapshots match existing controller; atomicity, supersession, stop, authorization and replay tests passed\n";
}
