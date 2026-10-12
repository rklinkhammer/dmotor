#include "dmotor/atomic_mailbox_top.hpp"
namespace dmotor {
void batch_transaction(AtomicMailbox& box, const uint32_t request[5], uint64_t tick,
                       const BatchAdmission& live, uint32_t reply[16]) {
    for (unsigned i=0;i<16;++i) reply[i]=0;
    VelocityBatch batch;
    switch (static_cast<BatchOperation>(request[0])) {
    case BatchOperation::reset: box.reset(live.session);reply[0]=1;break;
    case BatchOperation::write:
        // Reject bits outside the four AXI byte strobes rather than truncate them.
        if(request[3]<=15) reply[0]=box.write(request[1],request[2],static_cast<uint8_t>(request[3]),live);
        break;
    case BatchOperation::read: {
        const auto r=box.read(request[1]);reply[0]=r.okay;reply[1]=r.value;break;
    }
    case BatchOperation::begin:
        reply[0]=box.begin_tick(tick,live,batch);
        if(reply[0]) {
            reply[3]=batch.session;reply[4]=batch.sequence;reply[5]=batch.mask;
            reply[6]=static_cast<uint32_t>(batch.tick);reply[7]=static_cast<uint32_t>(batch.tick>>32);
            for(unsigned m=0;m<batch_reg::CHANNELS;++m){
                reply[8+m]=static_cast<uint32_t>(batch.velocity_mrpm[m]);reply[12+m]=batch.generation[m];
            }
        }
        break;
    case BatchOperation::retire: reply[0]=box.retire(request[1],request[2],live,request[3]!=0);break;
    case BatchOperation::invalidate: box.invalidate(request[1]);reply[0]=1;break;
    default: break;
    }
    reply[2]=static_cast<uint32_t>(box.rejection());
}
}
extern "C" void dmotor_atomic_mailbox(const uint32_t request[5], uint64_t tick,
                                      const uint32_t admission[11], uint32_t reply[16]) {
    static dmotor::AtomicMailbox box;
    dmotor::BatchAdmission live;
    live.session=admission[0];live.authorized_mask=admission[1];live.ready_mask=admission[2];
    for(unsigned m=0;m<dmotor::batch_reg::CHANNELS;++m){
        live.generation[m]=admission[3+m];live.max_speed_mrpm[m]=admission[7+m];
    }
    dmotor::batch_transaction(box,request,tick,live,reply);
}
