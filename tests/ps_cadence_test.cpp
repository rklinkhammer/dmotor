#include "dmotor/motion.hpp"
#include "dmotor/velocity.hpp"
#include "dmotor/timing.hpp"
#include <cassert>
#include <iostream>
int main(){
 using namespace dmotor;
 // Physical acceleration unchanged across compiled rates, with sub-milliRPM
 // remainder retained. Check the actual product fixed-point executor.
 MotionConfig c;c.generation=1;c.max_speed_mrpm=400000;c.acceleration_mrpm_s=12345;
 c.deceleration_mrpm_s=12345;c.counts_num=1000000;c.counts_den=10000;
 c.velocity_tolerance_mrpm=100;c.settle_ticks=12;c.kp_q16=655;c.ki_q16=65536;
 c.duty_limit_q16=65536;c.integral_limit_q16=65536;c.stop_timeout_us=1000000;
 MotionExecutor e;Descriptor command{};command[0]=1;command[2]=1;command[3]=1;command[4]=1;command[5]=100000;
 e.step(true,false,false,{},true,false,1,0,0,0,true,c);
 MotionResult r;
 for(unsigned i=1;i<=timing::control_hz;++i){
  r=e.step(false,true,i==1,command,true,false,1,uint64_t(i)*timing::control_tick_cycles,0,0,true,c);
  assert(!r.faults);
 }
 assert(r.ramp_mrpm==12345);
 // At the allowed event-rate bound, no 7-bit truncation in 5 ms mode.
 VelocityEstimator v;EncoderSample s{};
 const auto tick=timing::control_tick_cycles;
 v.step(false,tick,s,1,1000000,10000,2000000);
 s.count=timing::encoder_max_events_per_tick;s.last_edge=2*tick;
 v.step(false,2*tick,s,1,1000000,10000,2000000);
 s.count+=timing::encoder_max_events_per_tick;s.last_edge=3*tick;
 // Use a physically valid 400 RPM ratio at 100k events/sec: 15000 CPR.
 VelocityEstimator fast;EncoderSample f{};fast.step(false,tick,f,1,15000,1,2000000);
 f.count=timing::encoder_max_events_per_tick;f.last_edge=2*tick;fast.step(false,2*tick,f,1,15000,1,2000000);
 f.count+=timing::encoder_max_events_per_tick;f.last_edge=3*tick;
 auto result=fast.step(false,3*tick,f,1,15000,1,2000000);
 assert(!result.fault&&result.valid&&result.mrpm==400000);
 // Timestamp discontinuities are rejected rather than replaying missed work.
 result=fast.step(false,5*tick,f,1,15000,1,2000000);assert(result.fault);
 std::cout<<"PASS: "<<timing::control_period_us<<" us cadence, physical ramp scaling, maximum encoder event rate, missed-tick fault\n";
}
