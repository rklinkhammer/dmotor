#include "dmotor_sw/kernel_device.hpp"
#include "dmotor/controller_registers.hpp"
#include "driver/dmotor_ioctl.h"
#include <chrono>
#include <thread>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
namespace dmotor_sw {
namespace { void check(int result) { if(result<0) throw IoError(std::strerror(errno)); } }
KernelDevice::KernelDevice(const std::string& path) {
    fd_=::open(path.c_str(),O_RDWR|O_CLOEXEC|O_NOFOLLOW);
    check(fd_);
    try { if(read32(dmotor::controller_reg::IDENTITY)!=dmotor::controller_reg::PROFILE_ID) throw IoError("Unsupported device"); }
    catch(...) { ::close(fd_);fd_=-1;throw; }
}
KernelDevice::~KernelDevice() { if(fd_>=0) ::close(fd_); }
uint32_t KernelDevice::read32(uint32_t offset) { dmotor_access a{offset,0};check(ioctl(fd_,DMOTOR_READ,&a));return a.value; }
void KernelDevice::write32(uint32_t offset,uint32_t value) { dmotor_access a{offset,value};check(ioctl(fd_,DMOTOR_WRITE,&a)); }
void KernelDevice::fence() { if(read32(dmotor::controller_reg::IDENTITY)!=dmotor::controller_reg::PROFILE_ID) throw IoError("Device identity changed"); }
void KernelDevice::halt(bool asserted) { check(ioctl(fd_,asserted?DMOTOR_HALT:DMOTOR_RELEASE_HALT)); }
uint64_t KernelDevice::now_us() const { return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }
void KernelDevice::sleep_us(uint32_t duration) { std::this_thread::sleep_for(std::chrono::microseconds(duration)); }
}
