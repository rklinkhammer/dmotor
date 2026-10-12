#include "dmotor/batch_host.hpp"
#include <cstdlib>
#include <iostream>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while(false)
using namespace dmotor;
using namespace dmotor::batch_reg;
static BatchAdmission admission() {
    BatchAdmission a; a.session=7;a.authorized_mask=15;a.ready_mask=15;
    for(unsigned m=0;m<CHANNELS;++m){a.generation[m]=21+m;a.max_speed_mrpm[m]=200000;}
    return a;
}
static VelocityBatch frame(uint32_t seq,uint32_t mask) {
    VelocityBatch b;b.session=7;b.sequence=seq;b.mask=mask;
    for(unsigned m=0;m<CHANNELS;++m){b.generation[m]=21+m;b.velocity_mrpm[m]=(m&1)?-200000:200000;}
    return b;
}
struct Io {
    AtomicMailbox box;
    BatchAdmission live=admission();
    unsigned fences=0,writes=0;
    bool compatible=true;
    bool change_on_read=false;
    Io(){box.reset(live.session);}
    bool write(uint32_t offset,uint32_t value){++writes;return box.write(offset,value,15,live);}
    uint32_t read(uint32_t offset){
        if(offset==IDENTITY && !compatible)return 0;
        if(offset==APPLIED_TICK_HI && change_on_read){box.reset(8);change_on_read=false;}
        const auto r=box.read(offset);CHECK(r.okay);return r.value;
    }
    void fence(){++fences;}
};
int main(){
    Io io;VelocityBatch out;AppliedBatchSnapshot snap;
    CHECK(!io.box.read(3).okay);CHECK(!io.box.read(0x7c).okay);
    CHECK(!io.box.write(IDENTITY,0,15,io.live));
    CHECK(!io.box.write(STAGE_MASK,1,3,io.live));
    CHECK(!io.box.write(STAGE_MASK+1,1,15,io.live));
    CHECK(io.read(STAGE_MASK)==0);
    CHECK(!io.write(COMMIT,1));CHECK(io.box.rejection()==BatchReject::incomplete);
    io.compatible=false;CHECK(!publish_velocity_batch(io,frame(1,3)));io.compatible=true;
    CHECK(publish_velocity_batch(io,frame(1,3)));CHECK(io.fences==2);
    // Different masks, overlapping or disjoint, preserve the accepted pending frame.
    CHECK(!publish_velocity_batch(io,frame(2,6)));CHECK(io.box.rejection()==BatchReject::pending_mask);
    CHECK(!publish_velocity_batch(io,frame(2,12)));CHECK(io.read(PENDING_SEQUENCE)==1);
    CHECK(publish_velocity_batch(io,frame(3,3))); // Same-mask replacement.
    CHECK(!publish_velocity_batch(io,frame(3,3)));CHECK(io.box.rejection()==BatchReject::sequence);
    CHECK(!publish_velocity_batch(io,frame(2,3)));
    CHECK(io.write(STAGE_SESSION,7)); // Partial staging cannot alter pending.
    io.live.ready_mask=1;CHECK(!io.box.begin_tick(100,io.live,out));CHECK(io.read(PENDING_SEQUENCE)==3);
    io.live.ready_mask=15;CHECK(io.box.begin_tick(0x100000002ULL,io.live,out));
    CHECK(out.mask==3 && out.sequence==3 && out.velocity_mrpm[1]==-200000);
    CHECK(io.read(APPLIED_SEQUENCE)==0);CHECK(!io.box.begin_tick(1,io.live,out));
    CHECK(!io.box.retire(7,2,io.live,true));CHECK(io.read(INFLIGHT_SEQUENCE)==3);
    CHECK(io.box.retire(7,3,io.live,true));CHECK(read_applied_batch(io,snap));
    CHECK(snap.sequence==3 && snap.mask==3 && snap.tick==0x100000002ULL);
    CHECK(io.read(APPLIED_MOTOR_BASE)==3);
    CHECK(io.read(APPLIED_MOTOR_BASE+APPLIED_MOTOR_STRIDE)==3);
    CHECK(io.read(APPLIED_MOTOR_BASE+2*APPLIED_MOTOR_STRIDE)==0);
    CHECK(io.read(APPLIED_MOTOR_BASE+4)==2 && io.read(APPLIED_MOTOR_BASE+8)==1);
    CHECK(!io.box.read(APPLIED_MOTOR_BASE+12).okay);
    CHECK(!io.box.retire(7,3,io.live,true)); // Duplicate retirement.
    // Selected generation/range failures reject all, not merely the bad channel.
    auto b=frame(4,15);b.generation[3]=0;CHECK(!publish_velocity_batch(io,b));
    b=frame(4,15);b.velocity_mrpm[2]=200001;CHECK(!publish_velocity_batch(io,b));
    b.velocity_mrpm[2]=INT32_MIN;CHECK(!publish_velocity_batch(io,b));
    b=frame(4,1);b.generation[3]=0;b.velocity_mrpm[3]=INT32_MIN;
    CHECK(publish_velocity_batch(io,b)); // Unselected fields ignored.
    ++io.live.generation[0];CHECK(!io.box.begin_tick(200,io.live,out));
    CHECK(io.read(PENDING_SEQUENCE)==0);--io.live.generation[0];
    CHECK(publish_velocity_batch(io,frame(5,15)));io.live.authorized_mask=7;
    CHECK(!io.box.begin_tick(200,io.live,out));CHECK(io.read(PENDING_SEQUENCE)==0);
    io.live.authorized_mask=15;
    // Stop/fault intersects the whole pending or reserved group.
    CHECK(publish_velocity_batch(io,frame(6,3)));io.box.invalidate(2);
    CHECK(!io.box.begin_tick(200,io.live,out));
    CHECK(publish_velocity_batch(io,frame(7,3)));CHECK(io.box.begin_tick(200,io.live,out));
    CHECK(publish_velocity_batch(io,frame(8,3)));io.box.invalidate(1);
    CHECK(!io.box.retire(7,7,io.live,true));CHECK(io.read(PENDING_SEQUENCE)==0);
    CHECK(io.read(APPLIED_SEQUENCE)==3);
    // Failure after reservation also invalidates newer intersecting pending intent.
    CHECK(publish_velocity_batch(io,frame(9,3)));CHECK(io.box.begin_tick(201,io.live,out));
    CHECK(publish_velocity_batch(io,frame(10,3)));CHECK(!io.box.retire(7,9,io.live,false));
    CHECK(io.read(PENDING_SEQUENCE)==0 && io.read(APPLIED_SEQUENCE)==3);
    CHECK(publish_velocity_batch(io,frame(11,3)));CHECK(io.box.begin_tick(202,io.live,out));
    ++io.live.generation[1];CHECK(!io.box.retire(7,11,io.live,true));--io.live.generation[1];
    // Exhaust every legal mask and each selected/unselected invalidation bit.
    unsigned cases=0;
    for(uint32_t mask=1;mask<16;++mask)for(unsigned m=0;m<CHANNELS;++m){
        Io x;CHECK(publish_velocity_batch(x,frame(1,mask)));
        x.box.invalidate(1U<<m);
        CHECK(x.box.begin_tick(100,x.live,out)==!(mask & (1U<<m)));++cases;
    }
    CHECK(!publish_velocity_batch(io,frame(12,0)));CHECK(!publish_velocity_batch(io,frame(12,16)));
    CHECK(publish_velocity_batch(io,frame(UINT32_MAX,15)));
    CHECK(!publish_velocity_batch(io,frame(1,15))); // No sequence wrap.
    CHECK(io.box.begin_tick(203,io.live,out));io.box.reset(8);
    CHECK(!io.box.retire(7,UINT32_MAX,io.live,true));
    CHECK(!publish_velocity_batch(io,frame(1,15))); // Old-session writer.
    Io changing;CHECK(publish_velocity_batch(changing,frame(1,1)));
    CHECK(changing.box.begin_tick(0x1ffffffffULL,changing.live,out));
    CHECK(changing.box.retire(7,1,changing.live,true));
    changing.change_on_read=true;CHECK(!read_applied_batch(changing,snap));
    std::cout << "atomic mailbox: publication/retirement races, host ABI checks, and "
              << cases << " mask invalidation cases passed\n";
}
