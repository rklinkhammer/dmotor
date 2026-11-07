// Link actual PsDevice against a test-only KernelDevice implementation. No fd IO.
#include "ps_device.hpp"
#include "dmotor/timing.hpp"
#include <mutex>
#include <cassert>
#include <iostream>
#include <chrono>
#include <thread>
namespace {
std::mutex lock;
bool halted=true,attached=false,fail_read=false;
unsigned releases=0;
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
void KernelDevice::halt(bool a){std::lock_guard<std::mutex>g(lock);halted=a;if(!a)++releases;}
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
 std::cout<<"PASS: actual threaded PS backend starts inhibited, attaches without enabling, latches worker failure, blocks HALT release, joins on shutdown\n";
}
