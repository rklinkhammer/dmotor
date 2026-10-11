#include "dmotor/velocity.hpp"
#include "dmotor/detail/arithmetic.hpp"
#include "dmotor/timing.hpp"
namespace dmotor {
VelocityResult VelocityEstimator::step(bool reset, uint64_t now, const EncoderSample& s,
                                       uint32_t generation, uint32_t num, uint32_t den,
                                       uint32_t timeout) {
    if (reset) {
        *this = VelocityEstimator{};
        return result_;
    }
    // Bounds keep all conversion products in uint64_t. A call represents a
    // coherent 1 kHz snapshot; at most 100 accepted events can occur per tick.
    if (!generation || !num || num > 1000000 || !den || den > 10000 ||
        timeout < timing::zero_timeout_min_cycles || timeout > timing::zero_timeout_max_cycles) {
        result_.fault |= 1;
    }
    if (initialized_ &&
        (generation != generation_ || num != num_ || den != den_ || timeout != timeout_ ||
         now - previous_now_ != timing::control_tick_cycles)) {
        result_.fault |= 2;
    }
    if (result_.fault) {
        result_.valid = false;
        result_.mrpm = 0;
        return result_;
    }
    if (!initialized_) {
        initialized_ = true;
        started_ = now;
        previous_now_ = now;
        generation_ = generation;
        num_ = num;
        den_ = den;
        timeout_ = timeout;
        previous_ = s;
        // Establish a baseline only; an old pre-reset edge is not a measurement.
        have_edge_ = false;
        return result_;
    }
    previous_now_ = now;
    const uint64_t edges = s.count - previous_.count;
    const bool changed = s.direction_epoch != previous_.direction_epoch ||
                         s.invalid_transitions != previous_.invalid_transitions ||
                         s.negative != previous_.negative;
    if (s.invalid_transitions == UINT32_MAX || edges > 100 ||
        (edges && now - s.last_edge > timing::control_tick_cycles) ||
        (!edges && s.last_edge != previous_.last_edge)) {
        result_.fault |= 4;
    }
    result_.age = have_edge_ ? now - previous_.last_edge : now - started_;
    if (changed) {
        result_.valid = false;
        result_.mrpm = 0;
        have_edge_ = false;
    }
    if (edges && !result_.fault) {
        const uint64_t interval = s.last_edge - previous_.last_edge;
        if (have_edge_ && !changed) {
            if (interval < edges * timing::encoder_min_edge_cycles ||
                interval > timing::encoder_max_interval_cycles) {
                result_.fault |= 4;
            } else {
                // Edge-anchored count/time: one edge is reciprocal period;
                // several edges average their periods without a tick-window bias.
                // These casts follow all range checks: edges <= 100, den <=
                // 10000, num <= 1000000, interval <= UINT32_MAX. Multiply
                // the small factors first; the exact rational value is unchanged.
                const uint32_t scaled_edges = (static_cast<uint32_t>(edges) & 0x7fU) *
                                              (den & 0x3fffU);
                const uint64_t numerator =
                    static_cast<uint64_t>(scaled_edges) * (60ULL * reg::FABRIC_HZ * 1000);
                const uint64_t denominator =
                    static_cast<uint64_t>(static_cast<uint32_t>(interval)) * (num & 0xfffffU);
                const uint64_t speed = detail::divide(numerator + denominator / 2, denominator);
                if (speed > 1000000) {
                    result_.fault |= 8;
                } else {
                    result_.mrpm =
                        s.negative ? -static_cast<int32_t>(speed) : static_cast<int32_t>(speed);
                    result_.valid = true;
                    result_.averaged = edges > 1;
                }
            }
        }
        if (!have_edge_ || changed) {
            result_.valid = false;
            result_.mrpm = 0;
            result_.averaged = false;
        }
        have_edge_ = true;
        result_.age = now - s.last_edge;
    }
    if (changed && !edges) {
        started_ = now;
        result_.age = 0;
    }
    previous_ = s;
    result_.timed_out = result_.age >= timeout;
    if (result_.timed_out) {
        result_.mrpm = 0;
        result_.valid = true;
        result_.averaged = false;
        have_edge_ = false;
        // Anchor zero qualification at the last observation, not each later tick.
        started_ = now - result_.age;
    }
    if (result_.fault) {
        result_.mrpm = 0;
        result_.valid = false;
        result_.timed_out = false;
    }
    return result_;
}
} // namespace dmotor
extern "C" void dmotor_velocity(bool reset, uint64_t now, uint64_t count, uint64_t last_edge,
                                uint32_t direction_epoch, uint32_t invalid_transitions,
                                bool negative, uint32_t generation, uint32_t counts_num,
                                uint32_t counts_den, uint32_t zero_timeout_cycles, int32_t& mrpm,
                                uint64_t& age, uint32_t& status) {
    static dmotor::VelocityEstimator estimator;
    const dmotor::EncoderSample s{count, last_edge, direction_epoch, invalid_transitions, negative};
    const auto r =
        estimator.step(reset, now, s, generation, counts_num, counts_den, zero_timeout_cycles);
    mrpm = r.mrpm;
    age = r.age;
    status =
        (r.valid ? 1U : 0U) | (r.timed_out ? 2U : 0U) | (r.averaged ? 4U : 0U) | (r.fault << 8);
}
