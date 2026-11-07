#pragma once
#include "dmotor_sw/kernel_device.hpp"
#include <atomic>
#include <thread>
namespace dmotor_ps {
// Owns one kernel fd, one controller executor and one explicit worker lifetime.
// Kernel ioctls serialize individual accesses. Worker touches only PSF1 aperture;
// Runtime alone owns all ordinary control/configuration/authorization registers.
class PsDevice final : public dmotor_sw::Platform {
    dmotor_sw::KernelDevice device_;
    std::atomic<bool> stopping_{false}, failed_{false};
    std::thread thread_;
    void check() const;

  public:
    explicit PsDevice(const std::string& path);
    ~PsDevice() override;
    uint32_t read32(uint32_t) override;
    void write32(uint32_t, uint32_t) override;
    void fence() override;
    void halt(bool) override;
    uint64_t now_us() const override;
    void sleep_us(uint32_t) override;
};
} // namespace dmotor_ps
