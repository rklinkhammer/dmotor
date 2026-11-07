#pragma once
#include "dmotor/controller.hpp"
namespace dmotor {
class VelocityMailbox {
  public:
    void service(bool reset, const uint32_t request[4], bool tick, bool authorized,
                 bool stop, bool capture_fault, uint32_t session, uint64_t now,
                 const EncoderSample& encoder, const ControllerConfig& config,
                 uint32_t reply[16]);
  private:
    Controller controller_{};
    ControllerResult last_{};
    // Mutable staging and immutable pending bank. Active target lives in controller.
    uint32_t staged_[4] = {}; // session, generation, sequence, signed milliRPM
    uint32_t pending_sequence_ = 0, pending_generation_ = 0, pending_session_ = 0;
    int32_t pending_speed_ = 0;
    uint32_t published_sequence_ = 0, applied_sequence_ = 0, rejects_ = 0;
    uint8_t written_ = 0;
    bool pending_ = false, stopping_ = false, stop_pending_ = false;
    bool capture_pending_ = false;
};
}
extern "C" void dmotor_mailbox(bool reset, const uint32_t request[4], bool tick,
    bool authorized, bool stop, bool capture_fault, uint32_t session, uint64_t now,
    uint64_t count, uint64_t last_edge, uint32_t direction_epoch,
    uint32_t invalid_transitions, bool negative, const uint32_t configuration[20],
    uint32_t reply[16]);
