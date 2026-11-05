#pragma once
#include "dmotor_sw/session_link.hpp"
namespace dmotor_sw {
namespace monitor {
// Host behavioral simulator, not firmware: synthetic cause masks and serialized
// events. sample() must precede receive() for simultaneous fault/message events.
class Simulator {
  public:
    Frame boot(uint64_t durable_lifetime);
    void sample(uint64_t now_us, uint32_t active_causes, bool arm_pressed);
    Frame receive(const Frame& frame, uint64_t now_us);
    State state() const {
        return state_;
    }
    bool permits() const {
        return state_ == State::armed;
    }
    uint32_t epoch() const {
        return epoch_;
    }
    uint32_t latched_causes() const {
        return latched_;
    }
    unsigned commits() const {
        return commits_;
    }

  private:
    enum class Button { released_needed, press, press_debounce, release_debounce, qualify };
    bool clock(uint64_t now);
    bool healthy(uint64_t now) const;
    void fault(uint32_t causes);
    void clear_exchange();
    Frame respond(const Frame& request, Type type, Nak reason = Nak::wrong_state);
    SessionLink link_;
    State state_ = State::boot;
    Button button_ = Button::released_needed;
    uint64_t now_ = 0, sample_at_ = 0, button_at_ = 0, pending_at_ = 0;
    uint32_t active_ = 0, latched_ = 0, epoch_ = 0, transaction_ = 0, pl_session_ = 0;
    uint32_t last_transaction_ = 0;
    unsigned healthy_checks_ = 0, commits_ = 0;
    bool was_pressed_ = false;
    bool sampled_ = false, pending_ = false, completed_ = false, locked_ = false;
};
} // namespace monitor
} // namespace dmotor_sw
