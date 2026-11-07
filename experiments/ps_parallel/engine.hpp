#pragma once
#include "dmotor/mailbox.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
namespace dmotor_ps {
using Record = std::array<uint32_t,35>;
using Reply = std::array<uint32_t,16>;
// Each owner has its own four contexts. No static state shared between devices.
class Engine {
    std::array<dmotor::VelocityMailbox,4> contexts_{};
    std::array<bool,4> initialized_{};
    static uint64_t wide(const Record& r,unsigned i) {return r[i]|(uint64_t(r[i+1])<<32);}
public:
    Reply execute(const Record& r) {
        if(r[0]>=4 || (r[1]&~63U)) throw std::runtime_error("Invalid PS engine record");
        const unsigned m=r[0]; const auto flags=r[1];
        if(!(flags&1) && !initialized_[m]) throw std::runtime_error("Context requires PL cleanup/reset");
        Reply reply{};
        const dmotor::EncoderSample s{wide(r,5),wide(r,7),r[9],r[10],bool(flags&32)};
        const auto config=dmotor::decode_controller_config(r.data()+15);
        if(r[11]==3) {
            // Fuse the adapter's four staging writes + commit + tick into one
            // PS crossing. Execute the original operations in their exact order.
            if((flags&11)!=2 || r[14]) throw std::runtime_error("Invalid fused invocation");
            const uint32_t data[4]={r[2],r[15],r[12],r[13]};
            for(unsigned i=0;i<4;++i) {
                const uint32_t request[4]={1,i,data[i],0};
                contexts_[m].service(false,request,false,flags&4,false,flags&16,
                    r[2],wide(r,3),s,config,reply.data());
                if(reply[0]!=1 || reply[8] || reply[9]) return reply;
            }
            const uint32_t commit[4]={2,0,0,0};
            contexts_[m].service(false,commit,false,flags&4,false,flags&16,
                r[2],wide(r,3),s,config,reply.data());
            if(reply[0]!=1 || reply[8] || reply[9]) return reply;
            const uint32_t tick_request[4]={0,0,0,0};
            contexts_[m].service(false,tick_request,true,flags&4,false,flags&16,
                r[2],wide(r,3),s,config,reply.data());
        } else {
            contexts_[m].service(flags&1,r.data()+11,flags&2,flags&4,flags&8,flags&16,
                r[2],wide(r,3),s,config,reply.data());
        }
        if(flags&1) initialized_[m]=true;
        return reply;
    }
};
struct Io {
    virtual ~Io()=default;
    virtual uint32_t read(uint32_t)=0;
    virtual void write(uint32_t,uint32_t)=0;
};
// Only the daemon/backend owns this transport. No client socket or ARM operations.
class Worker {
    Io& io_; Engine engine_;
    uint64_t last_=0;
public:
    explicit Worker(Io& io):io_(io) {
        if(io_.read(0x4000)!=0x50534631) throw std::runtime_error("PSF1 capability required");
    }
    void attach() {
        io_.write(0x4024,1);
        if(io_.read(0x4024)!=1) throw std::runtime_error("Executor attach failed");
    }
    bool poll() {
        if(io_.read(0x4024)!=1) throw std::runtime_error("PL reset: explicit executor restart required");
        const auto status=io_.read(0x4008);
        if(status&2) return false; // Existing Runtime owns explicit fault handling/reset.
        if(!(status&1)) return false;
        const uint64_t ticket=io_.read(0x4010)|(uint64_t(io_.read(0x4014))<<32);
        if(!ticket||ticket<=last_) throw std::runtime_error("Replayed executor ticket");
        Record record{};
        for(unsigned i=0;i<35;++i) record[i]=io_.read(0x4080+i*4);
        // Cancellation/reset or replacement while reading must not mutate PS state.
        if(io_.read(0x4010)!=uint32_t(ticket)||io_.read(0x4014)!=uint32_t(ticket>>32)||
           !(io_.read(0x4008)&1)) return false; // PL cancellation is authoritative.
        Engine candidate=engine_;
        const auto reply=candidate.execute(record);
        io_.write(0x4018,uint32_t(ticket));io_.write(0x401c,uint32_t(ticket>>32));
        for(unsigned i=0;i<16;++i) io_.write(0x4140+i*4,reply[i]);
        io_.write(0x4020,1);
        const uint64_t accepted=io_.read(0x4028)|(uint64_t(io_.read(0x402c))<<32);
        if(accepted!=ticket) return false; // Discard speculative context on cancellation.
        engine_=candidate;last_=ticket;return true;
    }
};
}
