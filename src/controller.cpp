#include "dmotor/controller.hpp"
#include "dmotor/timing.hpp"
namespace dmotor {
namespace {
// Configuration, capture, decoder, estimator, acquisition and motion faults.
constexpr uint32_t controller_fault_mask = 1U | 2U | 4U | 8U | 16U | 32U;
// Canonical words, never native object bytes or padding. Reserved input words
// already invalidate generation in decode_controller_config.
void config_words(const ControllerConfig& c, uint32_t words[17]) {
    const auto& m = c.motion;
    words[0] = m.generation;
    words[1] = static_cast<uint32_t>(m.max_speed_mrpm);
    words[2] = m.acceleration_mrpm_s;
    words[3] = m.deceleration_mrpm_s;
    words[4] = m.counts_num;
    words[5] = m.counts_den;
    words[6] = m.position_tolerance;
    words[7] = m.velocity_tolerance_mrpm;
    words[8] = m.settle_ticks;
    words[9] = static_cast<uint32_t>(m.kp_q16);
    words[10] = static_cast<uint32_t>(m.ki_q16);
    words[11] = static_cast<uint32_t>(m.duty_limit_q16);
    words[12] = static_cast<uint32_t>(m.integral_limit_q16);
    words[13] = m.stop_timeout_us;
    words[14] = c.zero_timeout_cycles;
    words[15] = c.acquisition_timeout_cycles;
    words[16] = static_cast<uint32_t>(c.acquisition_duty_limit_q16);
}
} // namespace
void Controller::prepare(const ControllerConfig& c) {
    dmotor::config_words(c, prepared_words_);
    prepared_motion_ = prepare_motion_config(c.motion);
    prepared_acquisition_valid_ =
        c.acquisition_timeout_cycles >= timing::acquisition_timeout_min_cycles &&
        c.acquisition_timeout_cycles < c.zero_timeout_cycles &&
        c.acquisition_duty_limit_q16 >= 0 &&
        c.acquisition_duty_limit_q16 <= c.motion.duty_limit_q16;
    prepared_ = true;
}
bool Controller::matches(const ControllerConfig& c) const {
    uint32_t words[config_words] = {};
    dmotor::config_words(c, words);
    bool equal = true;
    for (unsigned i = 0; i < config_words; ++i) {
        equal = equal && words[i] == prepared_words_[i];
    }
    return equal;
}

ControllerResult Controller::step(bool reset, bool authorized, bool capture_fault, uint64_t now,
                                  const EncoderSample& encoder, int64_t position, uint32_t session,
                                  bool command_valid, const Descriptor& command,
                                  bool completion_ack, bool immediate_stop,
                                  const ControllerConfig& c) {
    return step_impl(reset, authorized, capture_fault, now, encoder, position, session,
                     command_valid, command, completion_ack, immediate_stop, c, false);
}
ControllerResult Controller::step_impl(bool reset, bool authorized, bool capture_fault,
                                       uint64_t now, const EncoderSample& encoder, int64_t position,
                                       uint32_t session, bool command_valid,
                                       const Descriptor& command, bool completion_ack,
                                       bool immediate_stop, const ControllerConfig& c,
                                       bool command_checked) {
    if (reset) {
        *this = Controller{};
        prepare(c);
        return {};
    }
    faults_ &= controller_fault_mask;
    ControllerResult r;
    const bool diagnostic_changed = primed_ && encoder.invalid_transitions != invalid_;
    const bool config_changed = !prepared_ || !matches(c);
    if (!authorized) {
        if (config_changed) {
            // Scaling/timeouts establish a fresh measurement epoch while disabled.
            velocity_ = VelocityEstimator{};
            primed_ = false;
            qualified_ = acquiring_ = false;
            prepare(c);
        }
    } else if (config_changed) {
        // Configuration changed without disabled preparation.
        faults_ |= 1;
    }
    if (!prepared_acquisition_valid_) {
        faults_ |= 1;
    }
    if (capture_fault) {
        faults_ |= 2;
    }
    if (diagnostic_changed) {
        faults_ |= 4;
    }
    invalid_ = encoder.invalid_transitions;
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
    r.motion =
        motion_.step_impl(false, authorized && !faults_, command_valid, command, completion_ack,
                          immediate_stop, session, now, position, r.velocity.mrpm, r.velocity.valid,
                          c.motion, pending, command_checked, prepared_motion_);
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
