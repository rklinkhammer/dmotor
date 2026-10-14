#pragma once
#include "dmotor_sw/device.hpp"
#include <cstddef>
#include <memory>
namespace dmotor_sw {
// Inhibited bring-up only. Cooperative flock ownership, not a kernel-enforced
// exclusive-open driver. AXI aborts/removal can terminate the process (SIGBUS).
class LinuxInspection final : public Device {
public:
    static std::unique_ptr<LinuxInspection> open_uio(unsigned index);
    // Takes ownership of fd, including on failure. Also permits regular-file
    // fixtures in tests. No regular-file/mock CLI mode is provided.
    LinuxInspection(int fd, size_t map_size, size_t map_offset);
    ~LinuxInspection() override;
    LinuxInspection(const LinuxInspection&) = delete;
    LinuxInspection& operator=(const LinuxInspection&) = delete;
    uint32_t read32(uint32_t offset) override;
    void write32(uint32_t offset, uint32_t value) override;
    void fence() override;
private:
    uint32_t load(uint32_t offset) const;
    void require_disabled() const;
    void cleanup();
    int fd_ = -1;
    void* mapping_ = nullptr;
    size_t size_ = 0, offset_ = 0;
};
}
