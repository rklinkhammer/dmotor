#include "dmotor/control.hpp"
#include "dmotor/timing.hpp"
#include "dmotor/detail/bounded.hpp"
namespace dmotor {
static int32_t clamp(int64_t value, int32_t limit) {
    return value > limit ? limit : value < -limit ? -limit : static_cast<int32_t>(value);
}
// Products are validated before entry and never reach the signed minimum.
// Expose the magnitude width to constant-division synthesis while preserving
// round-to-nearest with ties away from zero.
template <unsigned Bits> static int64_t rounded(int64_t value, uint64_t divisor) {
    constexpr uint64_t magnitude_mask = (uint64_t{1} << (Bits - 1)) - 1;
    const uint64_t magnitude = static_cast<uint64_t>(value < 0 ? -value : value) & magnitude_mask;
    const int64_t quotient = static_cast<int64_t>((magnitude + divisor / 2) / divisor);
    return value < 0 ? -quotient : quotient;
}
PiResult pi_step(PiConfig c, int32_t target, int32_t measured, int32_t integral, uint32_t period) {
    // Bounds ensure |ki * error * period| <= 2.098e17, inside signed 64 bits.
    if (c.kp_q16 < 0 || c.kp_q16 > 1048576 || c.ki_q16 < 0 || c.ki_q16 > 1048576 ||
        c.duty_limit_q16 < 0 || c.duty_limit_q16 > 65536 || c.integral_limit_q16 < 0 ||
        c.integral_limit_q16 > 65536 || target < -1000000 || target > 1000000 ||
        measured < -1000000 || measured > 1000000 || integral < -c.integral_limit_q16 ||
        integral > c.integral_limit_q16 || period < timing::pi_period_min_us ||
        period > timing::pi_period_max_us) {
        return {0, 0, true};
    }
    // Validated bounds: error +/-2000000 (22 signed bits), gains <= 2^20
    // (21 unsigned bits), period <= 100000 (17 unsigned bits).
    const int64_t error = detail::narrow_signed<22>(static_cast<int64_t>(target) - measured);
    const uint32_t kp = static_cast<uint32_t>(c.kp_q16) & 0x1fffffU;
    const uint32_t ki = static_cast<uint32_t>(c.ki_q16) & 0x1fffffU;
    const uint32_t duration = period & 0x1ffffU;
    const int64_t proportional_product = detail::narrow_signed<42>(kp * error);
    const int64_t integral_product = detail::narrow_signed<59>(ki * error * duration);
    const int64_t proportional = rounded<42>(proportional_product, 1000);
    const int32_t candidate =
        clamp(integral + rounded<59>(integral_product, 1000000000), c.integral_limit_q16);
    const int64_t candidate_duty = proportional + candidate;
    if ((candidate_duty <= c.duty_limit_q16 && candidate_duty >= -c.duty_limit_q16) ||
        (candidate_duty > c.duty_limit_q16 && error < 0) ||
        (candidate_duty < -c.duty_limit_q16 && error > 0)) {
        integral = candidate;
    }
    return {clamp(proportional + integral, c.duty_limit_q16), integral, false};
}
} // namespace dmotor
extern "C" void dmotor_pi(int32_t kp_q16, int32_t ki_q16, int32_t limit_q16,
                          int32_t integral_limit_q16, int32_t target_mrpm, int32_t measured_mrpm,
                          int32_t integral_q16, uint32_t period_us, int32_t& duty_out,
                          int32_t& integral_out, bool& range_fault) {
    const auto result = dmotor::pi_step({kp_q16, ki_q16, limit_q16, integral_limit_q16},
                                        target_mrpm, measured_mrpm, integral_q16, period_us);
    duty_out = result.duty_q16;
    integral_out = result.integral_q16;
    range_fault = result.range_fault;
}
