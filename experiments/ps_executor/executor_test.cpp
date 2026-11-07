#include "executor.hpp"
#include <cassert>
#include <iostream>
#include <limits>
using namespace ps_trial;
int main(){
 std::array<Config,4> config{};
 Executor e(7,config);Snapshot s;Frame f;std::array<double,4> targets{{30,60,-30,-60}};
 for(unsigned k=1;k<=80;++k){s.token=k;s.tick=uint64_t(k)*period;
  assert(e.step(s,targets,false,f));assert(f.token==k&&f.sequence==k&&f.session==7);
  for(auto d:f.duty)assert(d>=-32768&&d<=32768);
 }
 assert(f.duty[0]>0&&f.duty[2]<0);
 // Variable intervals within the budget; signed per-wheel estimator.
 for(unsigned k=81;k<100;++k){s.token=k;s.tick+=(k%2?80000:120000);
  for(unsigned i=0;i<4;++i){s.enc[i].count+=2;s.enc[i].edge=s.tick;s.enc[i].negative=i>=2;}
  assert(e.step(s,targets,false,f));
 }
 assert(e.velocity()[0]>0&&e.velocity()[2]<0);
 Frame sentinel=f;s.token++;s.tick+=max_interval+1;
 assert(!e.step(s,targets,false,f));assert(f.sequence==sentinel.sequence&&e.failed());
 s.tick+=period;s.token++;assert(!e.step(s,targets,false,f));
 Executor bad(8,config);Snapshot t;t.token=1;
 targets[0]=std::numeric_limits<double>::quiet_NaN();assert(!bad.step(t,targets,false,f));
 Executor stopped(9,config);targets={{20,20,20,20}};
 for(unsigned k=1;k<=300;++k){t.token=k;t.tick=uint64_t(k)*period;
  assert(stopped.step(t,targets,k==100,f));}
 for(auto d:f.duty)assert(d==0); // STOP remains latched despite subsequent targets
 Executor replay(10,config);assert(replay.step(t,targets,false,f));assert(!replay.step(t,targets,false,f));
 std::cout<<"PASS: C++14 four-wheel signed PI/ramp, variable dt, stale/replay/NaN rejection, sticky STOP/failure\n";
}
