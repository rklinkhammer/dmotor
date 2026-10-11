#pragma once
#include "dmotor/protocol.hpp"
#include <cstdint>
namespace dmotor {
struct MotionConfig {
    uint32_t generation = 0;
    int32_t max_speed_mrpm = 0;
    uint32_t acceleration_mrpm_s = 0, deceleration_mrpm_s = 0;
    uint32_t counts_num = 0, counts_den = 0;
    uint32_t position_tolerance = 0, velocity_tolerance_mrpm = 0, settle_ticks = 0;
    int32_t kp_q16 = 0, ki_q16 = 0, duty_limit_q16 = 0, integral_limit_q16 = 0;
    uint32_t stop_timeout_us = 0;
};
struct MotionResult {
    int32_t duty_q16 = 0, ramp_mrpm = 0;
    bool ready = false, busy = false, completion_valid = false, accepted = false;
    uint32_t sequence = 0, epoch = 0, outcome = 0, detail = 0, faults = 0;
    uint64_t activation = 0, terminal = 0;
};
// Exactly one call per 1 kHz control boundary, from a hardware scheduler.
// Feedback is an already-qualified estimator snapshot; invalid closed-loop
// feedback faults instead of guessing velocity, unless the internal coordinator
// explicitly supplies bounded feedback_pending qualification. Configuration stays stable
// throughout an enabled session. Completion is held until acknowledged.
class MotionExecutor {
  public:
    MotionResult step(bool reset, bool authorized, bool command_valid, const Descriptor& command,
                      bool completion_ack, bool immediate_stop, uint32_t session, uint64_t now,
                      int64_t position, int32_t velocity_mrpm, bool velocity_valid,
                      const MotionConfig& config, bool feedback_pending = false);

  private:
    friend class Controller;
    MotionResult step_impl(bool reset, bool authorized, bool command_valid,
                           const Descriptor& command, bool completion_ack, bool immediate_stop,
                           uint32_t session, uint64_t now, int64_t position, int32_t velocity_mrpm,
                           bool velocity_valid, const MotionConfig& config, bool feedback_pending,
                           bool command_checked);
    MotionResult result_{};
    uint8_t opcode_ = 0; // Validated opcodes 1..6, or inactive 0.
    uint32_t settling_ = 0; // Stops incrementing at settle_ticks <= 1000000.
    uint32_t timeout_us_ = 0;
    uint8_t motion_mode_ = 0; // 0 coast, 1 velocity PI, 2 bounded open-loop.
    int32_t target_mrpm_ = 0; // Validated +/-1000000 milliRPM.
    int32_t open_duty_ = 0; // Clamped to +/-65536, including both endpoints.
    int32_t speed_cap_ = 0; // Validated positive speed <= 1000000.
    int64_t target_position_ = 0;
    int64_t integral_nano_ = 0; // Clamped to +/-65536 * 1000000000.
    uint64_t started_ = 0, duration_us_ = 0, previous_tick_ = 0;
    uint16_t ramp_remainder_ = 0; // Modulo 1000, always 0..999.
    bool tick_primed_ = false, stopping_ = false;
    uint32_t session_ = 0, generation_ = 0;
    void finish(uint64_t now, uint32_t outcome, uint32_t detail);
    void fail(uint64_t now, uint32_t cause);
};
} // namespace dmotor
// Internal HLS kernel protocol; not public MMIO. Configuration words are
// documented in docs/motion_executor.md. Reply layout is declared there too.
extern "C" void dmotor_motion(bool reset, bool authorized, bool command_valid,
                              const uint32_t command[16], const uint32_t configuration[16],
                              bool completion_ack, bool immediate_stop, uint32_t session,
                              uint64_t now, int64_t position, int32_t velocity_mrpm,
                              bool velocity_valid, uint32_t reply[16]);
