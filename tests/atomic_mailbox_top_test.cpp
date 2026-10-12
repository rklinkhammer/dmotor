#include "dmotor/atomic_mailbox_top.hpp"
#include <cstdlib>
#include <iostream>
using namespace dmotor;
using namespace dmotor::batch_reg;
static AtomicMailbox reference;
static BatchAdmission live;
static uint64_t tick=0;
static unsigned transactions=0;
static uint32_t result[16]={};
static void call(BatchOperation op,uint32_t a=0,uint32_t b=0,uint32_t c=0){
    const uint32_t request[5]={static_cast<uint32_t>(op),a,b,c,0};
    uint32_t admission[11]={live.session,live.authorized_mask,live.ready_mask};
    for(unsigned m=0;m<CHANNELS;++m){admission[3+m]=live.generation[m];admission[7+m]=live.max_speed_mrpm[m];}
    uint32_t expected[16]={};
    batch_transaction(reference,request,tick,live,expected);
    dmotor_atomic_mailbox(request,tick,admission,result);
    for(unsigned i=0;i<16;++i)if(result[i]!=expected[i]){
        std::cerr<<"Mismatch transaction "<<transactions<<" word "<<i<<'\n';std::exit(1);
    }
    ++transactions;
}
static void write(uint32_t address,uint32_t value){call(BatchOperation::write,address,value,15);}
static void publish(uint32_t seq,uint32_t mask){
    write(STAGE_SESSION,live.session);write(STAGE_SEQUENCE,seq);write(STAGE_MASK,mask);
    for(unsigned m=0;m<CHANNELS;++m)if(mask&(1U<<m)){
        write(STAGE_MOTOR_BASE+m*STAGE_MOTOR_STRIDE,live.generation[m]);
        write(STAGE_MOTOR_BASE+m*STAGE_MOTOR_STRIDE+4,m&1?static_cast<uint32_t>(-200000):200000);
    }
    write(COMMIT,1);
}
int main(){
    live.session=7;live.authorized_mask=live.ready_mask=15;
    for(unsigned m=0;m<CHANNELS;++m){live.generation[m]=21+m;live.max_speed_mrpm[m]=200000;}
    call(BatchOperation::reset);
    uint32_t seq=1;
    for(uint32_t mask=1;mask<16;++mask){
        publish(seq++,mask);publish(seq++,mask); // Replacement before activation.
        live.ready_mask=0;call(BatchOperation::begin);live.ready_mask=15;
        tick+=0x100000001ULL;call(BatchOperation::begin);
        call(BatchOperation::retire,7,seq-1,1);
        for(uint32_t address=0;address<=INFLIGHT_SEQUENCE;address+=4)call(BatchOperation::read,address);
        for(unsigned m=0;m<CHANNELS;++m)for(unsigned f=0;f<4;++f)
            call(BatchOperation::read,APPLIED_MOTOR_BASE+m*APPLIED_MOTOR_STRIDE+f*4);
        publish(seq++,mask);call(BatchOperation::begin);
        call(BatchOperation::invalidate,mask);call(BatchOperation::retire,7,seq-1,1);
        publish(seq++,mask);live.authorized_mask=0;call(BatchOperation::begin);live.authorized_mask=15;
    }
    publish(seq++,15);call(BatchOperation::begin);publish(seq++,15);
    call(BatchOperation::retire,7,seq-2,0);
    publish(seq++,15);++live.generation[2];call(BatchOperation::begin);
    call(BatchOperation::write,STAGE_SEQUENCE,seq,1);
    call(BatchOperation::write,STAGE_MASK+1,15,15);
    publish(UINT32_MAX,15);publish(1,15);call(BatchOperation::begin);
    ++live.session;call(BatchOperation::reset);call(BatchOperation::retire,7,UINT32_MAX,1);
    std::cout<<"atomic mailbox HLS: "<<transactions<<" transactions matched all 16 reply words\n";
}
