#pragma once
#include "executor.hpp"
namespace ps_trial {
// Prototype register adapter. Bind only to an explicitly selected PSE1 simulator
// or future dedicated driver. Never bind to the production DMI2 controller.
struct RegisterIo {
 virtual ~RegisterIo()=default;
 virtual uint32_t read(uint32_t offset)=0;
 virtual void write(uint32_t offset,uint32_t value)=0; // throws on AXI rejection
};
class Transport {
 RegisterIo& io_;
 uint64_t wide(uint32_t offset){uint64_t lo=io_.read(offset);return lo|(uint64_t(io_.read(offset+4))<<32);}
 public:
 explicit Transport(RegisterIo& io):io_(io){if(io_.read(0)!=0x50534531)throw std::runtime_error("Not experimental PSE1");}
 Snapshot capture(){
  io_.write(16,1);Snapshot s;s.token=io_.read(16);s.tick=wide(20);
  auto negative=io_.read(36);
  for(unsigned i=0;i<4;++i){auto base=128+i*32;auto bits=wide(base);
   s.position[i]=bits<=uint64_t(INT64_MAX)?int64_t(bits):-1-int64_t(~bits);
   s.enc[i].count=wide(base+8);
   s.enc[i].edge=wide(base+16);s.enc[i].epoch=io_.read(base+24);
   s.enc[i].invalid=io_.read(base+28);s.enc[i].negative=(negative&(1U<<i))!=0;}
  if(!s.token||io_.read(16)!=s.token)throw std::runtime_error("Snapshot changed");
  return s;
 }
 // Acceptance is not physical application. Query applied() separately.
 void publish(const Frame& f){
  io_.write(64,f.session);io_.write(68,f.sequence);io_.write(72,f.token);
  for(unsigned i=0;i<4;++i)io_.write(76+4*i,uint32_t(f.duty[i]));
  io_.write(92,1);
  if(io_.read(28)!=f.sequence)throw std::runtime_error("Frame not accepted");
 }
 uint32_t applied(){return io_.read(32);}
 // No constructor ARM, enable, refresh, automatic retry or recovery.
};
}
