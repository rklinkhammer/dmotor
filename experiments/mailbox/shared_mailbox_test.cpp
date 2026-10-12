#include "dmotor/shared_mailbox.hpp"
#include "dmotor/timing.hpp"
#include <cstdlib>
#include <iostream>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while(false)
struct Channel {
    dmotor::VelocityMailbox reference;
    uint64_t now = 0, count = 0, edge = 0;
    uint32_t session = 1, direction = 0;
    bool authorized = true, negative = false;
    uint32_t reply[16] = {};
    uint32_t config[20] = {1,200000,1000000,1000000,10296,5,1,100,2,655,
        65536,65536,65536,1000000,dmotor::timing::fabric_hz/10,
        dmotor::timing::fabric_hz*3/50,128,0,0,0};
};
static Channel channels[dmotor::mailbox_motor_count];
static unsigned compared = 0;
static void service(unsigned motor, uint32_t op=0, uint32_t field=0, uint32_t value=0,
                    bool tick=false, bool reset=false, bool stop=false, bool fault=false) {
    auto& c = channels[motor];
    uint32_t request[4] = {op,field,value,0}, expected[16] = {};
    c.reference.service(reset,request,tick,c.authorized,stop,fault,c.session,c.now,
        {c.count,c.edge,c.direction,0,c.negative},dmotor::decode_controller_config(c.config),expected);
    dmotor_mailbox_shared(motor,reset,request,tick,c.authorized,stop,fault,c.session,
        c.now,c.count,c.edge,c.direction,0,c.negative,c.config,c.reply);
    for(unsigned i=0;i<16;++i) {
        if(c.reply[i]!=expected[i]) {
            std::cerr << "motor=" << motor << " transaction=" << compared << " word=" << i
                      << " expected=" << expected[i] << " got=" << c.reply[i] << '\n';
            std::exit(1);
        }
    }
    ++compared;
}
static void tick(unsigned motor, bool stop=false, bool fault=false) {
    auto& c=channels[motor];
    c.now+=dmotor::timing::control_tick_cycles;
    service(motor,0,0,0,true,false,stop,fault);
}
static void publish(unsigned motor,uint32_t seq,int32_t speed) {
    auto& c=channels[motor];
    service(motor,1,0,c.session); service(motor,1,1,c.config[0]);
    service(motor,1,2,seq); service(motor,1,3,static_cast<uint32_t>(speed));
    service(motor,2);
}
int main() {
    for(unsigned m=0;m<dmotor::mailbox_motor_count;++m) {
        channels[m].session=11+m;
        channels[m].config[0]=21+m;
        channels[m].config[9]+=m*100; // Different PI configuration in each context.
        service(m,0,0,0,false,true);
    }
    for(unsigned t=0;t<110;++t)
        for(unsigned m=0;m<dmotor::mailbox_motor_count;++m) tick(3-m);
    // Interleave every staging word so bank isolation is exercised before commit.
    for(unsigned f=0;f<4;++f) for(unsigned m=0;m<4;++m) {
        const uint32_t data[4]={channels[m].session,channels[m].config[0],1,(m+1)*1000};
        service(m,1,f,data[f]);
    }
    for(unsigned m=0;m<4;++m) {service(m,2);tick(m);CHECK(channels[m].reply[2]==1);}
    // Different encoder rates and target signs, repeated switching between contexts.
    for(unsigned t=0;t<160;++t) {
        for(unsigned k=0;k<4;++k) {
            unsigned m=(k+t)%4;
            auto& c=channels[m];
            if(t%(m+1)==0){++c.count;c.edge=c.now+dmotor::timing::control_tick_cycles;}
            if(t==20) publish(m,2,static_cast<int32_t>((m+1)*2000));
            if(t==40) {publish(m,3,3000);publish(m,4,4000);}
            if(t==60) publish(m,4,1000); // Replay must not alter active target.
            if(t==80 && m==1) {c.negative=true;++c.direction;publish(m,5,-5000);}
            tick(m);
        }
    }
    // Reset one context while other channels retain active state and sequence history.
    ++channels[2].session; service(2,0,0,0,false,true);
    for(unsigned m=0;m<4;++m) if(m!=2) {service(m);CHECK(channels[m].reply[2]>=4);}
    // Independent stop, authorization loss and captured fault. Shared HALT distribution
    // belongs to the external safety wrapper; then model its authorization fanout.
    service(0,0,0,0,false,false,true);tick(0);
    channels[1].authorized=false;service(1);CHECK(channels[1].reply[4]==0);
    service(3,0,0,0,false,false,false,true);CHECK(channels[3].reply[4]==0);
    for(unsigned m=0;m<4;++m){channels[m].authorized=false;tick(m);CHECK(channels[m].reply[4]==0);}
    // Invalid selectors cannot alias a real motor, including a reset request.
    uint32_t request[4]={}, reply[16]={};
    for(uint32_t bad : {4U,UINT32_MAX}) {
        dmotor_mailbox_shared(bad,true,request,true,true,true,true,99,0,0,0,0,0,
                              false,channels[0].config,reply);
        CHECK(reply[9]==16);for(unsigned i=0;i<16;++i)if(i!=9)CHECK(reply[i]==0);
    }
    for(unsigned m=0;m<4;++m)service(m);
    // Reinitialize all channels and exercise pending work during warmup separately.
    for(unsigned m=0;m<4;++m){channels[m].authorized=true;++channels[m].session;
        service(m,0,0,0,false,true);publish(m,1,1000);}
    for(unsigned t=0;t<110;++t)for(unsigned m=0;m<4;++m)tick(m);
    std::cout << "shared mailbox: " << compared
              << " interleaved transactions, all 16 reply words match four independent contexts\n";
}
