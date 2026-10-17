// Test-only replacement, never linked or installed with the deployed service.
#include "dmotor_sw/kernel_device.hpp"
#include "dmotor/controller_registers.hpp"
#include "dmotor/batch_registers.hpp"
#include <map>
#include <chrono>
#include <thread>
namespace dmotor_sw {
namespace r=dmotor::controller_reg;
namespace b=dmotor::batch_reg;
namespace { std::map<uint32_t,uint32_t> words;bool reset=true,source=true; }
KernelDevice::KernelDevice(const std::string&) {
    words.clear();words[r::IDENTITY]=r::PROFILE_ID;words[r::STATUS]=1U<<18;
    words[b::IDENTITY]=b::PROFILE_ID;words[b::SCHEMA]=1;words[b::MOTOR_COUNT]=4;
}
KernelDevice::~KernelDevice() { halt(true); }
uint32_t KernelDevice::read32(uint32_t a) { return words[a]; }
void KernelDevice::write32(uint32_t a,uint32_t v) {
    if(a==r::SESSION) words[b::SESSION]=v;
    if(a==r::CORE_RESET) { reset=v!=0;if(reset) words[r::STATUS]|=1U<<18;else if(!source) words[r::STATUS]&=~(1U<<18); }
    if(a==r::ENABLE) words[r::STATUS]|=v|(v<<8);
    if(a==r::DISABLE) words[r::STATUS]&=~(v|(v<<8));
    if(a==r::STOP) words[r::STATUS]|=v<<12;
    if(a==b::COMMIT) { words[b::PUBLISHED_SEQUENCE]=words[b::STAGE_SEQUENCE];words[b::APPLIED_SEQUENCE]=words[b::STAGE_SEQUENCE]; }
    words[a]=v;
}
void KernelDevice::fence() {}
void KernelDevice::halt(bool value) { source=value;if(value) { words[r::STATUS]&=~15U;words[r::STATUS]|=1U<<18; } }
uint64_t KernelDevice::now_us() const { return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }
void KernelDevice::sleep_us(uint32_t us) { std::this_thread::sleep_for(std::chrono::microseconds(us)); }
}
