#include "engine.hpp"
#include "dmotor/timing.hpp"
#include <cassert>
#include <iostream>
int main(){
 dmotor_ps::Engine fused,serial;
 dmotor_ps::Record r{};r[1]=1;r[2]=7;
 const uint32_t cfg[20]={21,200000,1000000,1000000,10296,5,1,100,2,655,65536,65536,65536,1000000,2000000,1200000,128,0,0,0};
 for(unsigned i=0;i<20;++i)r[15+i]=cfg[i];
 for(unsigned m=0;m<4;++m){r[0]=m;assert(fused.execute(r)==serial.execute(r));}
 unsigned compared=0;
 for(unsigned t=1;t<=250;++t)for(unsigned m=0;m<4;++m){
  r[0]=m;r[1]=6;r[3]=t*dmotor::timing::control_tick_cycles;r[11]=r[12]=r[13]=r[14]=0;
  if(t>110 && t%3==0){
   auto one=r;one[11]=3;one[12]=t;one[13]=uint32_t((m&1)?-1000:1000);
   r[1]=4;
   const uint32_t values[4]={r[2],r[15],t,one[13]};
   for(unsigned i=0;i<4;++i){r[11]=1;r[12]=i;r[13]=values[i];serial.execute(r);}
   r[11]=2;r[12]=r[13]=0;serial.execute(r);r[11]=0;r[1]=6;
   assert(fused.execute(one)==serial.execute(r));++compared;
  }else{assert(fused.execute(r)==serial.execute(r));}
 }
 std::cout<<"PASS: "<<compared<<" fused four-stage/commit/tick operations match serial original operations, all reply words\n";
}
