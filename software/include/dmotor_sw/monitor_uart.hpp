#pragma once
#include "dmotor_sw/monitor_protocol.hpp"
#include <string>
namespace dmotor_sw {
namespace monitor {
// One owner, one bounded TX slot. Caller supplies monotonic deadlines and must
// schedule poll independently of the motor executor. No recovery authority here.
class Uart {
  public:
    explicit Uart(const std::string& path);
    ~Uart();
    Uart(const Uart&) = delete;
    Uart& operator=(const Uart&) = delete;
    void queue(const Frame& frame, uint64_t deadline_us);
    bool poll(uint64_t now_us, Frame& received);
    bool transmitting() const {
        return sent_ < size_;
    }
    uint64_t rejected() const {
        return rejected_;
    }

  private:
    int fd_ = -1;
    Decoder decoder_;
    std::array<uint8_t, max_encoded> tx_{};
    std::size_t sent_ = 0, size_ = 0;
    uint64_t deadline_ = 0, rejected_ = 0;
};
} // namespace monitor
} // namespace dmotor_sw
