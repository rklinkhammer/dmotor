#include "dmotor/motion.hpp"
#include "dmotor/detail/bounded.hpp"
#include "dmotor/registers.hpp"
namespace dmotor {
namespace detail {
// Explicit function boundaries allow the Vitis adapter to share exact dividers.
// Callers validate nonzero divisors and bound rounding additions before entry.

uint64_t divide(uint64_t numerator, uint64_t denominator) {
    return numerator / denominator;
}

int64_t rounded(int64_t value, int64_t denominator) {
    const bool negative = value < 0;
    const uint64_t absolute =
        negative ? uint64_t{0} - static_cast<uint64_t>(value) : static_cast<uint64_t>(value);
    const uint64_t divisor = static_cast<uint64_t>(denominator);
    const int64_t quotient = static_cast<int64_t>(divide(absolute + divisor / 2, divisor));
    return negative ? -quotient : quotient;
}
} // namespace detail

namespace {
constexpr uint32_t ramp_ticks_per_second = 1000;
constexpr uint64_t clocks_per_tick = reg::FABRIC_HZ / 1000;
constexpr uint64_t clocks_per_us = reg::FABRIC_HZ / 1000000;
constexpr int64_t integral_scale = 1000000000LL;
int64_t magnitude(int32_t value) {
    return value < 0 ? -static_cast<int64_t>(value) : value;
}
int32_t clamp(int64_t value, int32_t limit) {
    return value > limit ? limit : value < -limit ? -limit : static_cast<int32_t>(value);
}
bool valid_config(const MotionConfig& c) {
    return c.generation && c.max_speed_mrpm >= 1000 && c.max_speed_mrpm <= 1000000 &&
           c.acceleration_mrpm_s && c.acceleration_mrpm_s <= 10000000 && c.deceleration_mrpm_s &&
           c.deceleration_mrpm_s <= 10000000 && c.counts_num && c.counts_num <= 1000000 &&
           c.counts_den && c.counts_den <= 1000000 &&
           static_cast<uint64_t>(c.max_speed_mrpm) * c.counts_num <= 6000000000ULL * c.counts_den &&
           c.velocity_tolerance_mrpm <= static_cast<uint32_t>(c.max_speed_mrpm) && c.settle_ticks &&
           c.settle_ticks <= 1000000 && c.kp_q16 >= 0 && c.kp_q16 <= 1048576 && c.ki_q16 >= 0 &&
           c.ki_q16 <= 1048576 && c.duty_limit_q16 >= 0 && c.duty_limit_q16 <= 65536 &&
           c.integral_limit_q16 >= 0 && c.integral_limit_q16 <= 65536 && c.stop_timeout_us;
}
} // namespace
void MotionExecutor::finish(uint64_t now, uint32_t outcome, uint32_t detail) {
    result_.busy = false;
    result_.completion_valid = true;
    result_.outcome = outcome;
    result_.detail = detail;
    result_.terminal = now;
    opcode_ = 0;
}
void MotionExecutor::fail(uint64_t now, uint32_t cause) {
    result_.faults |= cause;
    result_.duty_q16 = 0;
    result_.ramp_mrpm = 0;
    motion_mode_ = 0;
    integral_nano_ = 0;
    ramp_remainder_ = 0;
    stopping_ = false;
    if (result_.busy) {
        finish(now, 2, cause);
    }
}
MotionResult MotionExecutor::step(bool reset, bool authorized, bool command_valid,
                                  const Descriptor& command, bool completion_ack,
                                  bool immediate_stop, uint32_t session, uint64_t now,
                                  int64_t position, int32_t velocity_mrpm, bool velocity_valid,
                                  const MotionConfig& c, bool feedback_pending) {
    return step_impl(reset, authorized, command_valid, command, completion_ack, immediate_stop,
                     session, now, position, velocity_mrpm, velocity_valid, c, feedback_pending,
                     false);
}
MotionResult MotionExecutor::step_impl(bool reset, bool authorized, bool command_valid,
                                       const Descriptor& command, bool completion_ack,
                                       bool immediate_stop, uint32_t session, uint64_t now,
                                       int64_t position, int32_t velocity_mrpm, bool velocity_valid,
                                       const MotionConfig& c, bool feedback_pending,
                                       bool command_checked) {
    if (reset) {
        *this = MotionExecutor{};
        return result_;
    }
    result_.accepted = false;
    if (completion_ack) {
        result_.completion_valid = false;
    }
    if (!authorized) {
        if (result_.busy) {
            finish(now, 2, 0x100); // Ring cancellation remains authoritative.
        }
        result_.duty_q16 = 0;
        result_.ramp_mrpm = 0;
        result_.ready = false;
        motion_mode_ = 0;
        stopping_ = false;
        opcode_ = 0;
        integral_nano_ = 0;
        ramp_remainder_ = 0;
        tick_primed_ = false;
        return result_;
    }
    if (!valid_config(c) || session == 0) {
        fail(now, 1);
    }
    if (tick_primed_ && (now - previous_tick_ != clocks_per_tick || session != session_ ||
                         c.generation != generation_)) {
        fail(now, 2);
    }
    tick_primed_ = true;
    previous_tick_ = now;
    session_ = session;
    generation_ = c.generation;
    if (result_.faults) {
        result_.ready = false;
        return result_;
    }
    if (immediate_stop && !stopping_) {
        if (result_.busy) {
            finish(now, 2, 0x200);
        }
        stopping_ = true;
        opcode_ = 0;
        target_mrpm_ = 0;
        motion_mode_ = 1;
        started_ = now;
        timeout_us_ = c.stop_timeout_us;
        settling_ = 0;
        // Transfer open-loop to a speed ramp using measured speed.
        if (velocity_valid) {
            result_.ramp_mrpm = velocity_mrpm;
        }
    }
    const bool can_accept = !result_.busy && !result_.completion_valid && !stopping_ &&
                            !(feedback_pending && !velocity_valid);
    if (command_valid && can_accept && !immediate_stop) {
        result_.accepted = true;
        result_.sequence = command[2];
        result_.epoch = command[3];
        result_.activation = now;
        result_.busy = true;
        started_ = now;
        settling_ = 0;
        const Error error = command_checked ? Error::none
                                            : validate(command, command[2], session, c.generation,
                                                       position, c.max_speed_mrpm);
        if (error != Error::none) {
            result_.activation = 0;
            finish(now, 1, static_cast<uint32_t>(error));
        } else if ((command[0] == 1 || command[0] == 2 || command[0] == 3 || command[0] == 6) &&
                   (!velocity_valid || magnitude(velocity_mrpm) > c.max_speed_mrpm)) {
            // Reject unsafe activation before an immediate command can complete.
            fail(now, 8);
        } else {
            opcode_ = static_cast<uint8_t>(command[0] & 7U);
            if ((opcode_ == 1 || opcode_ == 2 || opcode_ == 3 || opcode_ == 6) &&
                motion_mode_ != 1) {
                result_.ramp_mrpm = velocity_mrpm;
                ramp_remainder_ = 0;
            }
            timeout_us_ = command[11];
            if (opcode_ == 1) {
                target_mrpm_ =
                    static_cast<int32_t>(detail::narrow_signed<21>(signed_word(command[5])));
                motion_mode_ = 1;
                finish(now, 0, 0);
            } else if (opcode_ == 4) {
                open_duty_ = static_cast<int32_t>(detail::narrow_signed<18>(
                    clamp(signed_word(command[8]), c.duty_limit_q16)));
                motion_mode_ = 2;
                result_.ramp_mrpm = 0;
                ramp_remainder_ = 0;
                integral_nano_ = 0;
                finish(now, 0, 0);
            } else if (opcode_ == 2 || opcode_ == 3) {
                target_position_ = signed_pair(command[6], command[7]);
                if (opcode_ == 3) {
                    target_position_ += position; // validate proved no overflow.
                }
                speed_cap_ =
                    static_cast<int32_t>(detail::narrow_signed<21>(signed_word(command[5])));
                motion_mode_ = 1;
            } else if (opcode_ == 5) {
                duration_us_ = (static_cast<uint64_t>(command[10]) << 32) | command[9];
            } else if (opcode_ == 6) {
                if (motion_mode_ == 2) {
                    result_.ramp_mrpm = velocity_mrpm;
                }
                target_mrpm_ = 0;
                motion_mode_ = 1;
            }
        }
    }
    // Expiration wins settling on the boundary. Divide elapsed ticks to avoid
    // overflowing a 64-bit dwell duration when converting it to fabric cycles.
    const uint64_t elapsed_us = detail::divide(now - started_, clocks_per_us);
    if ((stopping_ || opcode_ == 2 || opcode_ == 3 || opcode_ == 6) && elapsed_us >= timeout_us_) {
        fail(now, 4);
    }
    if (result_.faults) {
        result_.ready = false;
        return result_;
    }
    // Only the bounded coordinator may authorize this hold. Do not fabricate
    // speed, advance the profile/integral, or complete settling while unqualified.
    if (feedback_pending && !velocity_valid) {
        result_.ready = false;
        settling_ = 0;
        if (stopping_ || opcode_ == 6) {
            result_.duty_q16 = 0;
        }
        return result_;
    }
    if (opcode_ == 5 && elapsed_us >= duration_us_) {
        finish(now, 0, 0);
    }
    bool in_position = false;
    if (opcode_ == 2 || opcode_ == 3) {
        const bool forward = target_position_ >= position;
        const uint64_t distance =
            forward ? static_cast<uint64_t>(target_position_) - static_cast<uint64_t>(position)
                    : static_cast<uint64_t>(position) - static_cast<uint64_t>(target_position_);
        in_position = distance <= c.position_tolerance;
        const uint64_t speed = static_cast<uint64_t>(magnitude(result_.ramp_mrpm));
        const uint64_t denominator = 120000ULL * c.deceleration_mrpm_s * c.counts_den;
        const uint64_t stop_distance =
            detail::divide(speed * speed * c.counts_num + denominator - 1, denominator);
        const uint64_t step_denominator = 60000000ULL * c.counts_den;
        const uint64_t lookahead =
            detail::divide(speed * c.counts_num + step_denominator - 1, step_denominator);
        const bool moving_toward = (result_.ramp_mrpm >= 0) == forward;
        target_mrpm_ = (in_position || (moving_toward && distance <= stop_distance + lookahead)) ? 0
                       : forward ? speed_cap_
                                 : -speed_cap_;
    }
    if (motion_mode_ == 1) {
        if (!velocity_valid || magnitude(velocity_mrpm) > c.max_speed_mrpm) {
            fail(now, 8);
        } else {
            // Reversal decelerates to zero before accelerating the other way.
            int32_t goal = target_mrpm_;
            if ((goal < 0 && result_.ramp_mrpm > 0) || (goal > 0 && result_.ramp_mrpm < 0)) {
                goal = 0;
            }
            const bool decelerating = magnitude(goal) < magnitude(result_.ramp_mrpm);
            const uint32_t rate = decelerating ? c.deceleration_mrpm_s : c.acceleration_mrpm_s;
            // Validated rate <= 10000000; retained remainder < 1000.
            // The sum is <= 10000999, so a 32-bit datapath is sufficient.
            const uint32_t accumulated = ramp_remainder_ + rate;
            const int64_t advance = accumulated / ramp_ticks_per_second;
            ramp_remainder_ = static_cast<uint16_t>(accumulated % ramp_ticks_per_second);
            const int64_t change = static_cast<int64_t>(goal) - result_.ramp_mrpm;
            if (magnitude(static_cast<int32_t>(change)) <= advance) {
                result_.ramp_mrpm = goal;
                ramp_remainder_ = 0;
            } else {
                result_.ramp_mrpm +=
                    change > 0 ? static_cast<int32_t>(advance) : -static_cast<int32_t>(advance);
            }
            // Valid feedback/ramp: error +/-2000000; gains 0..1048576.
            const int64_t error = detail::narrow_signed<22>(
                static_cast<int64_t>(result_.ramp_mrpm) - velocity_mrpm);
            const uint32_t ki = static_cast<uint32_t>(c.ki_q16) & 0x1fffffU;
            const uint32_t kp = static_cast<uint32_t>(c.kp_q16) & 0x1fffffU;
            const int64_t limit = static_cast<int64_t>(c.integral_limit_q16) * integral_scale;
            int64_t candidate = integral_nano_ + static_cast<int64_t>(ki) * error * 1000;
            candidate = candidate > limit ? limit : candidate < -limit ? -limit : candidate;
            const int64_t proportional =
                detail::rounded(static_cast<int64_t>(kp) * error, 1000);
            const int64_t effort = proportional + detail::rounded(candidate, integral_scale);
            if ((effort <= c.duty_limit_q16 && effort >= -c.duty_limit_q16) ||
                (effort > c.duty_limit_q16 && error < 0) ||
                (effort < -c.duty_limit_q16 && error > 0)) {
                integral_nano_ = detail::narrow_signed<47>(candidate);
            }
            result_.duty_q16 = clamp(proportional + detail::rounded(integral_nano_, integral_scale),
                                     c.duty_limit_q16);
        }
    } else {
        result_.duty_q16 = motion_mode_ == 2 ? open_duty_ : 0;
    }
    const bool settling_command = stopping_ || opcode_ == 6 || opcode_ == 2 || opcode_ == 3;
    if (!result_.faults && settling_command) {
        const bool settled =
            velocity_valid && magnitude(velocity_mrpm) <= c.velocity_tolerance_mrpm &&
            result_.ramp_mrpm == 0 && ((opcode_ != 2 && opcode_ != 3) || in_position);
        settling_ = settled ? (settling_ + 1) & 0xfffffU : 0;
        if (settling_ >= c.settle_ticks) {
            motion_mode_ = 0;
            target_mrpm_ = 0;
            result_.duty_q16 = 0;
            integral_nano_ = 0;
            if (result_.busy) {
                finish(now, 0, 0);
            }
            stopping_ = false;
            opcode_ = 0;
        }
    }
    result_.ready = !result_.busy && !result_.completion_valid && !stopping_ && !result_.faults;
    return result_;
}
} // namespace dmotor
extern "C" void dmotor_motion(bool reset, bool authorized, bool command_valid,
                              const uint32_t command[16], const uint32_t configuration[16],
                              bool completion_ack, bool immediate_stop, uint32_t session,
                              uint64_t now, int64_t position, int32_t velocity_mrpm,
                              bool velocity_valid, uint32_t reply[16]) {
    static dmotor::MotionExecutor executor;
    dmotor::Descriptor descriptor{};
    for (unsigned i = 0; i < 16; ++i) {
        descriptor[i] = command[i];
    }
    dmotor::MotionConfig c;
    c.generation = configuration[0];
    c.max_speed_mrpm = dmotor::signed_word(configuration[1]);
    c.acceleration_mrpm_s = configuration[2];
    c.deceleration_mrpm_s = configuration[3];
    c.counts_num = configuration[4];
    c.counts_den = configuration[5];
    c.position_tolerance = configuration[6];
    c.velocity_tolerance_mrpm = configuration[7];
    c.settle_ticks = configuration[8];
    c.kp_q16 = dmotor::signed_word(configuration[9]);
    c.ki_q16 = dmotor::signed_word(configuration[10]);
    c.duty_limit_q16 = dmotor::signed_word(configuration[11]);
    c.integral_limit_q16 = dmotor::signed_word(configuration[12]);
    c.stop_timeout_us = configuration[13];
    if (configuration[14] || configuration[15]) {
        c.generation = 0;
    }
    const auto r =
        executor.step(reset, authorized, command_valid, descriptor, completion_ack, immediate_stop,
                      session, now, position, velocity_mrpm, velocity_valid, c);
    reply[0] = static_cast<uint32_t>(r.duty_q16);
    reply[1] = static_cast<uint32_t>(r.ramp_mrpm);
    reply[2] = (r.ready ? 1U : 0U) | (r.busy ? 2U : 0U) | (r.completion_valid ? 4U : 0U) |
               (r.accepted ? 8U : 0U);
    reply[3] = r.sequence;
    reply[4] = r.epoch;
    reply[5] = r.outcome;
    reply[6] = r.detail;
    reply[7] = r.faults;
    reply[8] = static_cast<uint32_t>(r.activation);
    reply[9] = static_cast<uint32_t>(r.activation >> 32);
    reply[10] = static_cast<uint32_t>(r.terminal);
    reply[11] = static_cast<uint32_t>(r.terminal >> 32);
    for (unsigned i = 12; i < 16; ++i) {
        reply[i] = 0;
    }
}
