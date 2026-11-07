#pragma once
// Experimental C++14 controller. No device access, recovery or production ABI.
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
namespace ps_trial {
constexpr uint64_t clock_hz=20000000, period=100000, max_interval=200000;
struct Encoder {uint64_t count=0, edge=0; uint32_t epoch=0, invalid=0; bool negative=false;};
struct Snapshot {uint32_t token=0; uint64_t tick=0; std::array<Encoder,4> enc{};
 std::array<int64_t,4> position{};};
struct Frame {uint32_t session=0,sequence=0,token=0; std::array<int32_t,4> duty{};};
struct Config {
 double counts_per_rev=2048, max_rpm=400, acceleration=60, deceleration=90;
 double kp=.0025,ki=.01,duty_limit=.5,integral_limit=.25;
 uint64_t zero_timeout=2000000;
};
class Executor {
 std::array<Config,4> cfg_;
 std::array<double,4> velocity_{},ramp_{},integral_{};
 std::array<bool,4> valid_{};
 Snapshot previous_{};
 uint32_t session_,sequence_=0;
 uint64_t start_=0;
 bool primed_=false,failed_=false,stopping_=false;
 static double bound(double x,double lim){return std::max(-lim,std::min(lim,x));}
 public:
 explicit Executor(uint32_t session,std::array<Config,4> configs):cfg_(configs),session_(session){
  if(!session)throw std::invalid_argument("zero session");
  for(const auto& c:cfg_)if(!std::isfinite(c.counts_per_rev)||c.counts_per_rev<=0||
    !std::isfinite(c.max_rpm)||c.max_rpm<=0||c.max_rpm>400||
    !std::isfinite(c.acceleration)||c.acceleration<=0||
    !std::isfinite(c.deceleration)||c.deceleration<=0||
    !std::isfinite(c.kp)||c.kp<0||!std::isfinite(c.ki)||c.ki<0||
    !std::isfinite(c.duty_limit)||c.duty_limit<0||c.duty_limit>1||
    !std::isfinite(c.integral_limit)||c.integral_limit<0||c.integral_limit>1||
    c.zero_timeout<max_interval||c.zero_timeout>200000000)
      throw std::invalid_argument("configuration");
 }
 bool failed()const{return failed_;}
 const std::array<double,4>& velocity()const{return velocity_;}
 // False means no frame may be published. Fault is sticky for this executor.
 // A new executor is NOT permission to rearm the physical device.
 bool step(const Snapshot& s,const std::array<double,4>& targets,bool stop,Frame& out){
  if(failed_)return false;
  auto fail=[&](){failed_=true;return false;};
  if(!s.token||s.token<=previous_.token||sequence_==UINT32_MAX)return fail();
  for(unsigned i=0;i<4;++i)if(!std::isfinite(targets[i])||
    std::abs(targets[i])>cfg_[i].max_rpm||s.enc[i].invalid||s.enc[i].edge>s.tick)return fail();
  if(primed_&&(s.tick<=previous_.tick||s.tick-previous_.tick>max_interval))return fail();
  if(!primed_)start_=s.tick;
  const double dt=primed_?double(s.tick-previous_.tick)/clock_hz:double(period)/clock_hz;
  if(stop&&!stopping_){stopping_=true;ramp_=velocity_;}
  Frame frame;frame.session=session_;frame.sequence=sequence_+1;frame.token=s.token;
  for(unsigned i=0;i<4;++i){
   const auto& e=s.enc[i];const auto& p=previous_.enc[i];const auto& c=cfg_[i];
   if(primed_&&e.count<p.count)return fail(); // counter wrap requires new epoch
   if(primed_&&e.epoch==p.epoch&&e.count>p.count){
    velocity_[i]=(e.negative?-1:1)*double(e.count-p.count)*60/(c.counts_per_rev*dt);
    valid_[i]=true;
   }else if(s.tick-start_>=c.zero_timeout&&s.tick-e.edge>=c.zero_timeout){
    velocity_[i]=0;valid_[i]=true;
   }else if(!primed_||e.epoch!=p.epoch){valid_[i]=false;velocity_[i]=0;}
   if(std::abs(velocity_[i])>c.max_rpm)return fail();
   // Conservative unqualified-feedback hold; production acquisition remains a gap.
   if(!valid_[i]){frame.duty[i]=0;continue;}
   double goal=stopping_?0:targets[i];
   if(goal*ramp_[i]<0)goal=0;
   const double rate=std::abs(goal)<std::abs(ramp_[i])?c.deceleration:c.acceleration;
   ramp_[i]+=bound(goal-ramp_[i],rate*dt);
   const double error=ramp_[i]-velocity_[i];
   const double candidate=bound(integral_[i]+c.ki*error*dt,c.integral_limit);
   const double effort=c.kp*error+candidate;
   if(std::abs(effort)<=c.duty_limit||effort*error<0)integral_[i]=candidate;
   double duty=bound(c.kp*error+integral_[i],c.duty_limit);
   if(stopping_&&ramp_[i]==0&&std::abs(velocity_[i])<1){duty=0;integral_[i]=0;}
   frame.duty[i]=int32_t(std::llround(duty*65536));
  }
  previous_=s;primed_=true;++sequence_;out=frame;return true;
 }
};
}
