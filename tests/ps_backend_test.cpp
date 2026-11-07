// Link actual PsDevice against a test-only KernelDevice implementation. No fd IO.
#include "dmotor_sw/ps_device.hpp"
#include "dmotor/timing.hpp"
#include <mutex>
#include <condition_variable>
#include <cassert>
#include <iostream>
#include <chrono>
#include <thread>
namespace {
std::mutex lock;
std::condition_variable changed;
bool halted=true,attached=false,fail_read=false;
bool block_release=false,release_entered=false,allow_release=false;
unsigned releases=0,assertions=0;
}
namespace dmotor_sw {
KernelDevice::KernelDevice(const std::string&){}
KernelDevice::~KernelDevice(){}
uint32_t KernelDevice::read32(uint32_t a){
 std::lock_guard<std::mutex> g(lock);
 if(fail_read)throw IoError("injected worker IO error");
 if(a==0x4000)return 0x50534631;
 if(a==0x4004)return dmotor::timing::control_period_us;
 if(a==0x4024)return attached;
 return 0;
}
void KernelDevice::write32(uint32_t a,uint32_t v){std::lock_guard<std::mutex>g(lock);if(a==0x4024)attached=v;}
void KernelDevice::halt(bool a){
 std::unique_lock<std::mutex>g(lock);
 if(!a&&block_release){release_entered=true;changed.notify_all();changed.wait(g,[]{return allow_release;});}
 halted=a;if(!a)++releases;else ++assertions;
 changed.notify_all();
}
void KernelDevice::fence(){}
uint64_t KernelDevice::now_us()const{return 0;}
void KernelDevice::sleep_us(uint32_t us){std::this_thread::sleep_for(std::chrono::microseconds(us));}
}
int main(){
 {
  dmotor_ps::PsDevice device("fixture");
  {std::lock_guard<std::mutex>g(lock);assert(halted&&attached&&releases==0);fail_read=true;}
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  bool failed=false;try{device.halt(false);}catch(const dmotor_sw::IoError&){failed=true;}
  assert(failed);
  {std::lock_guard<std::mutex>g(lock);assert(halted&&releases==0);}
 }
 {
  {std::lock_guard<std::mutex>g(lock);fail_read=false;attached=false;block_release=true;}
  dmotor_ps::PsDevice device("fixture");
  std::thread releasing([&]{device.halt(false);});
  unsigned before;
  {std::unique_lock<std::mutex>g(lock);
   assert(changed.wait_for(g,std::chrono::seconds(2),[]{return release_entered;}));
   before=assertions;fail_read=true;
  }
  // sleep_us checks the atomic failure latch without taking the control lock.
  bool failed=false;
  for(unsigned i=0;i<2000&&!failed;++i){
   try{device.sleep_us(1000);}catch(const dmotor_sw::IoError&){failed=true;}
  }
  assert(failed);
  // Failure HALT must wait behind the in-flight release, never precede it.
  {std::unique_lock<std::mutex>g(lock);
   assert(!changed.wait_for(g,std::chrono::milliseconds(20),[&]{return assertions!=before;}));
   allow_release=true;changed.notify_all();
  }
  releasing.join();
  {std::unique_lock<std::mutex>g(lock);
   assert(changed.wait_for(g,std::chrono::seconds(2),[&]{return assertions>before;}));
   assert(halted);
  }
  failed=false;try{device.halt(false);}catch(const dmotor_sw::IoError&){failed=true;}assert(failed);
 }
 std::cout<<"PASS: threaded PS backend starts inhibited, attaches without enabling, latches failure, serializes failure HALT after in-flight release, blocks later release, joins on shutdown\n";
}
