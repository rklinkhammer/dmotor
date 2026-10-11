#pragma once
#include <cstdint>
namespace dmotor {
// Coherent snapshot of accepted encoder events, never raw asynchronous pins.
struct EncoderSample {
    uint64_t count = 0, last_edge = 0;
    uint32_t direction_epoch = 0, invalid_transitions = 0;
    bool negative = false;
};
struct VelocityResult {
    int32_t mrpm = 0;
    uint64_t age = 0;
    bool valid = false, timed_out = false, averaged = false;
    uint32_t fault = 0;
};
class VelocityEstimator {
  public:
    // Timeout bounds are defined in dmotor/timing.hpp.
    VelocityResult step(bool reset, uint64_t now, const EncoderSample& sample, uint32_t generation,
                        uint32_t counts_num, uint32_t counts_den, uint32_t zero_timeout_cycles);

  private:
    EncoderSample previous_{};
    VelocityResult result_{};
    bool initialized_ = false, have_edge_ = false;
    uint64_t started_ = 0, previous_now_ = 0;
    uint32_t generation_ = 0, num_ = 0, den_ = 0, timeout_ = 0;
};
} // namespace dmotor
extern "C" void dmotor_velocity(bool reset, uint64_t now, uint64_t count, uint64_t last_edge,
                                uint32_t direction_epoch, uint32_t invalid_transitions,
                                bool negative, uint32_t generation, uint32_t counts_num,
                                uint32_t counts_den, uint32_t zero_timeout_cycles, int32_t& mrpm,
                                uint64_t& age, uint32_t& status);
