#pragma once
#include <cstdint>
namespace dmotor {
// HLS-friendly staged configuration and safety-intent decoder. Hardware must
// revalidate safety intents at retirement, against current protection state.
class Configuration {
  public:
    bool access(bool write, uint32_t address, uint32_t data, uint32_t safety_state, uint32_t faults,
                uint32_t live_faults, uint32_t expected_refresh, uint32_t ring_status,
                uint32_t producer, uint32_t consumer, uint32_t epoch, uint32_t& read_data,
                uint32_t& action, uint32_t& argument);
    uint32_t generation() const {
        return generation_;
    }
    uint32_t speed() const {
        return active_speed_;
    }
    uint64_t timeout_cycles() const;
    uint32_t refresh_session() const {
        return refresh_session_;
    }
    bool valid() const {
        return generation_ != 0;
    }

  private:
    uint32_t staged_timeout_ = 0, staged_speed_ = 130000;
    uint32_t active_timeout_ = 0, active_speed_ = 0;
    uint32_t generation_ = 0, refresh_session_ = 0;
};
} // namespace dmotor
