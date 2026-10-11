#include "dmotor/controller.hpp"
#include "dmotor/timing.hpp"
namespace dmotor {
ControllerResult Controller::step(bool reset, bool authorized, bool capture_fault, uint64_t now,
                                  const EncoderSample& encoder, int64_t position, uint32_t session,
                                  bool command_valid, const Descriptor& command,
                                  bool completion_ack, bool immediate_stop,
                                  const ControllerConfig& c) {
    if (reset) {
        *this = Controller{};
        return {};
    }
    ControllerResult r;
    if (c.acquisition_timeout_cycles < timing::acquisition_timeout_min_cycles ||
        c.acquisition_timeout_cycles >= c.zero_timeout_cycles || c.acquisition_duty_limit_q16 < 0 ||
        c.acquisition_duty_limit_q16 > c.motion.duty_limit_q16) {
        faults_ |= 1;
    }
    if (primed_ && (c.acquisition_timeout_cycles != acquisition_timeout_ ||
                    c.acquisition_duty_limit_q16 != acquisition_limit_)) {
        faults_ |= 1;
    }
    if (capture_fault) {
        faults_ |= 2;
    }
    if (primed_ && encoder.invalid_transitions != invalid_) {
        faults_ |= 4;
    }
    invalid_ = encoder.invalid_transitions;
    acquisition_timeout_ = c.acquisition_timeout_cycles;
    acquisition_limit_ = c.acquisition_duty_limit_q16;
    primed_ = true;
    r.velocity = velocity_.step(false, now, encoder, c.motion.generation, c.motion.counts_num,
                                c.motion.counts_den, c.zero_timeout_cycles);
    if (r.velocity.fault) {
        faults_ |= 8;
    }
    if (!authorized) {
        acquiring_ = false;
        qualified_ = false;
    } else if (!faults_) {
        if (qualified_ && !r.velocity.valid && !acquiring_) {
            acquiring_ = true;
            acquisition_start_ = now;
        }
        // Expiry wins even when a valid interval arrives on the deadline tick.
        if (acquiring_ && now - acquisition_start_ >= c.acquisition_timeout_cycles) {
            faults_ |= 16;
        }
        if (!faults_ && r.velocity.valid) {
            qualified_ = true;
            acquiring_ = false;
        }
    }
    r.acquiring = authorized && acquiring_ && !faults_;
    r.warming_up = authorized && !qualified_ && !faults_;
    const bool pending = r.acquiring || r.warming_up;
    r.motion = motion_.step(false, authorized && !faults_, command_valid, command, completion_ack,
                            immediate_stop, session, now, position, r.velocity.mrpm,
                            r.velocity.valid, c.motion, pending);
    if (r.motion.faults) {
        faults_ |= 32;
    }
    if (r.acquiring) {
        const int32_t limit = c.acquisition_duty_limit_q16;
        if (r.motion.duty_q16 > limit) {
            r.motion.duty_q16 = limit;
        }
        if (r.motion.duty_q16 < -limit) {
            r.motion.duty_q16 = -limit;
        }
    }
    if (r.warming_up || faults_ || !authorized) {
        r.motion.duty_q16 = 0;
    }
    if (faults_) {
        r.motion.ready = false;
        r.acquiring = false;
        r.warming_up = false;
    }
    r.faults = faults_;
    return r;
}
} // namespace dmotor
extern "C" void dmotor_controller(bool reset, bool authorized, bool capture_fault, uint64_t now,
                                  uint64_t count, uint64_t last_edge, uint32_t direction_epoch,
                                  uint32_t invalid_transitions, bool negative, int64_t position,
                                  uint32_t session, bool command_valid, const uint32_t command[16],
                                  bool completion_ack, bool immediate_stop,
                                  const uint32_t configuration[20], uint32_t reply[24]) {
    static dmotor::Controller controller;
    const auto c = dmotor::decode_controller_config(configuration);
    dmotor::Descriptor d{};
    for (unsigned i = 0; i < 16; ++i) {
        d[i] = command[i];
    }
    const dmotor::EncoderSample s{count, last_edge, direction_epoch, invalid_transitions, negative};
    const auto r = controller.step(reset, authorized, capture_fault, now, s, position, session,
                                   command_valid, d, completion_ack, immediate_stop, c);
    dmotor::encode_controller_result(r, reply);
}

dmotor::ControllerConfig dmotor::decode_controller_config(const uint32_t configuration[20]) {
    dmotor::ControllerConfig c;
    auto& m = c.motion;
    m.generation = configuration[0];
    m.max_speed_mrpm = dmotor::signed_word(configuration[1]);
    m.acceleration_mrpm_s = configuration[2];
    m.deceleration_mrpm_s = configuration[3];
    m.counts_num = configuration[4];
    m.counts_den = configuration[5];
    m.position_tolerance = configuration[6];
    m.velocity_tolerance_mrpm = configuration[7];
    m.settle_ticks = configuration[8];
    m.kp_q16 = dmotor::signed_word(configuration[9]);
    m.ki_q16 = dmotor::signed_word(configuration[10]);
    m.duty_limit_q16 = dmotor::signed_word(configuration[11]);
    m.integral_limit_q16 = dmotor::signed_word(configuration[12]);
    m.stop_timeout_us = configuration[13];
    c.zero_timeout_cycles = configuration[14];
    c.acquisition_timeout_cycles = configuration[15];
    c.acquisition_duty_limit_q16 = dmotor::signed_word(configuration[16]);
    if (configuration[17] || configuration[18] || configuration[19]) {
        m.generation = 0;
    }
    return c;
}
void dmotor::encode_controller_result(const ControllerResult& r, uint32_t reply[24]) {
    reply[0] = static_cast<uint32_t>(r.motion.duty_q16);
    reply[1] = static_cast<uint32_t>(r.motion.ramp_mrpm);
    reply[2] = (r.motion.ready ? 1U : 0U) | (r.motion.busy ? 2U : 0U) |
               (r.motion.completion_valid ? 4U : 0U) | (r.motion.accepted ? 8U : 0U);
    reply[3] = r.motion.sequence;
    reply[4] = r.motion.epoch;
    reply[5] = r.motion.outcome;
    reply[6] = r.motion.detail;
    reply[7] = r.motion.faults;
    reply[8] = static_cast<uint32_t>(r.motion.activation);
    reply[9] = static_cast<uint32_t>(r.motion.activation >> 32);
    reply[10] = static_cast<uint32_t>(r.motion.terminal);
    reply[11] = static_cast<uint32_t>(r.motion.terminal >> 32);
    reply[12] = static_cast<uint32_t>(r.velocity.mrpm);
    reply[13] = (r.velocity.valid ? 1U : 0U) | (r.velocity.timed_out ? 2U : 0U) |
                (r.velocity.averaged ? 4U : 0U) | (r.velocity.fault << 8);
    reply[14] = static_cast<uint32_t>(r.velocity.age);
    reply[15] = static_cast<uint32_t>(r.velocity.age >> 32);
    reply[16] = r.faults;
    reply[17] = (r.acquiring ? 1U : 0U) | (r.warming_up ? 2U : 0U);
    for (unsigned i = 18; i < 24; ++i) {
        reply[i] = 0;
    }
}
