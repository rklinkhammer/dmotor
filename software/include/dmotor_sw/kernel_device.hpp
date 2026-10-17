#pragma once
#include "dmotor_sw/runtime.hpp"
#include <string>
namespace dmotor_sw {
class KernelDevice final : public Platform {
public:
    explicit KernelDevice(const std::string& path);
    ~KernelDevice() override;
    KernelDevice(const KernelDevice&) = delete;
    KernelDevice& operator=(const KernelDevice&) = delete;
    uint32_t read32(uint32_t offset) override;
    void write32(uint32_t offset,uint32_t value) override;
    void fence() override;
    void halt(bool asserted) override;
    uint64_t now_us() const override;
    void sleep_us(uint32_t duration) override;
private: int fd_=-1;
};
}
