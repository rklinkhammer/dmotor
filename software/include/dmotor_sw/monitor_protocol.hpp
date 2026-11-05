#pragma once
#include "dmotor_sw/monitor_wire.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
namespace dmotor_sw {
namespace monitor {
// Shared PS/ESP32 codec: fixed storage, no OS, allocation or packed structs.
struct Frame {
    uint64_t source = 0, peer = 0;
    uint32_t epoch = 0, transaction = 0, sequence = 0;
    Type type = Type::hello;
    uint8_t length = 0;
    std::array<uint8_t, max_payload> payload{};
};
void put_le(uint8_t* destination, uint64_t value, std::size_t bytes);
uint64_t get_le(const uint8_t* source, std::size_t bytes);
uint16_t crc16(const uint8_t* bytes, std::size_t length);
bool valid(const Frame& frame);
std::size_t encode(const Frame& frame, std::array<uint8_t, max_encoded>& output);
enum class Decode { incomplete, frame, rejected };
class Decoder {
  public:
    Decode push(uint8_t byte, Frame& output);
    void reset();

  private:
    std::array<uint8_t, max_decoded> bytes_{};
    std::size_t used_ = 0;
    bool active_ = false, escaped_ = false, discard_ = false;
};
} // namespace monitor
} // namespace dmotor_sw
