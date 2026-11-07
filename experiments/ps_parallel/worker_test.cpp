#include "engine.hpp"
#include <cassert>
#include <map>
#include <iostream>
struct Fake:dmotor_ps::Io {
 std::map<uint32_t,uint32_t> regs;
 dmotor_ps::Record record{};
 dmotor_ps::Reply reply{};
 uint64_t ticket=1,accepted=0,echo=0;unsigned written=0;
 bool pending=true,cancel=false;
 Fake(){regs[0x4000]=0x50534631;record[1]=1;record[2]=9;record[15]=1;}
 uint32_t read(uint32_t a)override {
  if(a==0x4008)return pending?1:0;
  if(a==0x4010)return uint32_t(ticket);
  if(a==0x4014)return uint32_t(ticket>>32);
  if(a==0x4028)return uint32_t(accepted);
  if(a==0x402c)return uint32_t(accepted>>32);
  if(a>=0x4080&&a<0x410c)return record[(a-0x4080)/4];
  return regs[a];
 }
 void write(uint32_t a,uint32_t v)override {
  if(a==0x4018){echo=(echo&0xffffffff00000000ULL)|v;written|=1U<<16;}
  else if(a==0x401c){echo=(echo&0xffffffffULL)|(uint64_t(v)<<32);written|=1U<<17;}
  else if(a>=0x4140&&a<0x4180){auto i=(a-0x4140)/4;reply[i]=v;written|=1U<<i;}
  else if(a==0x4020){if(pending&&!cancel&&written==0x3ffff&&echo==ticket){accepted=ticket;pending=false;}written=0;}
  else regs[a]=v;
 }
};
int main(){
 Fake io;dmotor_ps::Worker w(io);w.attach();assert(w.poll());assert(io.accepted==1&&io.reply[13]==9);
 assert(!w.poll());
 io.ticket=2;io.pending=true;io.cancel=true;io.record[0]=1;assert(!w.poll());
 // Cancelled reset must not initialize the speculative context.
 io.ticket=3;io.cancel=false;io.record[1]=2;
 bool rejected=false;try{w.poll();}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 io.record[1]=1;assert(w.poll());
 io.regs[0x4024]=0;rejected=false;try{w.poll();}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 Fake bad;bad.regs[0x4000]=0x444d4932;rejected=false;try{dmotor_ps::Worker no(bad);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 std::cout<<"PASS: PS identity, complete packing, context isolation, speculative cancellation, accepted tickets, physical reset rejection\n";
}
