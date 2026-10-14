#pragma once
#include <cstdint>
#include <stdexcept>
namespace dmotor_sw {
// A single serialized owner must hold the backend for its entire lifetime.
// All offsets are peripheral-relative, aligned 32-bit accesses. Throw IoError
// for failed reads/writes/fences; never substitute zero for an unsuccessful read.
// Linux implementations must enforce device-memory mapping and completion ordering.
class IoError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
class Device {
public:
    virtual ~Device() = default;
    virtual uint32_t read32(uint32_t offset) = 0;
    virtual void write32(uint32_t offset, uint32_t value) = 0;
    virtual void fence() = 0;
};
} // namespace dmotor_sw
