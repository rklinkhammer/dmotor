// Fictional plant exercising the actual C++ executor. Not target timing evidence.
#include "executor.hpp"
#include <iostream>
#include <iomanip>
#include <deque>
#include <cstdlib>
using namespace ps_trial;
struct Delivery {uint64_t arrival,capture;Frame frame;};
int main(){
 for(unsigned hz:{1000U,200U})for(bool jitter:{false,true})for(bool stall:{false,true}){
  std::array<Config,4> cfg{};for(auto& c:cfg){c.acceleration=180;c.deceleration=270;c.duty_limit=1;c.integral_limit=1;}
  Executor executor(1,cfg);Snapshot snap;std::array<double,4> speed{},position{},output{};
  std::array<int64_t,4> whole{};std::array<bool,4> have_direction{};
  std::deque<Delivery> queue;uint64_t last_capture=0,next_release=0,release=0;
  bool inhibited=false;uint64_t inhibit_at=0;double square=0,max_speed=0;unsigned samples=0;
  uint32_t random=1;const uint64_t step=2000,nominal=clock_hz/hz;
  for(uint64_t now=0;now<6*clock_hz;now+=step){
   if(now-last_capture>=400000&&!inhibited){inhibited=true;inhibit_at=now;}
   if(now>=next_release){
    ++release;random=random*1664525U+1013904223U;
    int64_t offset=jitter?(int64_t(random%40001)-20000):0;
    int64_t planned=int64_t(release*nominal)+offset;
    next_release=uint64_t(std::max(int64_t(now+step),planned));
    if(!inhibited&&!(stall&&now>=2*clock_hz&&now<clock_hz*21/10)){
     snap.tick=now;++snap.token;Frame f;
     std::array<double,4> target{{60,90,-60,-90}};
     if(now<clock_hz*3/10)target.fill(0);
     if(!executor.step(snap,target,now>=4*clock_hz,f)){
      inhibited=true;inhibit_at=now;
     }else queue.push_back({now+10000,now,f}); // fictional 500 us compute/transport
    }
   }
   while(!queue.empty()&&queue.front().arrival<=now){
    auto d=queue.front();queue.pop_front();
    if(!inhibited&&now-d.capture<max_interval){
     last_capture=d.capture;for(unsigned i=0;i<4;++i)output[i]=double(d.frame.duty[i])/65536;
    }
   }
   for(unsigned i=0;i<4;++i){
    if(inhibited)output[i]=0;
    const double dt=double(step)/clock_hz,tau=inhibited?.4:(.08+.02*i);
    double derivative=(400*output[i]-speed[i])/tau;
    derivative=std::max(-720.0,std::min(720.0,derivative));speed[i]+=derivative*dt;
    position[i]+=speed[i]*cfg[i].counts_per_rev/60*dt;
    auto value=int64_t(std::floor(position[i]));auto delta=value-whole[i];whole[i]=value;
    if(delta){bool negative=delta<0;
     if(have_direction[i]&&negative!=snap.enc[i].negative)++snap.enc[i].epoch;
     have_direction[i]=true;snap.enc[i].negative=negative;
     snap.enc[i].count+=uint64_t(std::llabs(delta));snap.enc[i].edge=now;
    }
    max_speed=std::max(max_speed,std::abs(speed[i]));
    if(!stall&&now>clock_hz&&now<3*clock_hz){
     double goal=i==0?60:i==1?90:i==2?-60:-90;
     square+=(speed[i]-goal)*(speed[i]-goal);++samples;
    }
   }
  }
  if(stall){if(!inhibited||inhibit_at<2*clock_hz||inhibit_at>clock_hz*202/100)return 1;}
  else if(inhibited){std::cerr<<"Unexpected inhibit hz="<<hz<<" jitter="<<jitter<<" time="<<double(inhibit_at)/clock_hz<<'\n';return 2;}
  std::cout<<"hz="<<hz<<" jitter="<<jitter<<" stall="<<stall
   <<" tracking_rms_rpm="<<std::sqrt(square/std::max(1U,samples))
   <<" max_speed_rpm="<<max_speed<<" inhibit_s="<<double(inhibit_at)/clock_hz
   <<" end_speed_rpm="<<std::abs(speed[0])<<'\n';
 }
 std::cout<<"PASS: actual C++ executor, eight fictional four-motor plant/jitter/stall scenarios\n";
}
