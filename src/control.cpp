#include "dmotor/control.hpp"
namespace dmotor {
static int32_t clamp(int64_t value, int32_t limit) {
    return value > limit ? limit : value < -limit ? -limit : static_cast<int32_t>(value);
}
static int64_t rounded(int64_t value, int64_t divisor) {
    return value < 0 ? -((-value + divisor / 2) / divisor) : (value + divisor / 2) / divisor;
}
PiResult pi_step(PiConfig c, int32_t target, int32_t measured, int32_t integral, uint32_t period) {
    // Bounds ensure |ki * error * period| <= 2.098e17, inside signed 64 bits.
    if (c.kp_q16 < 0 || c.kp_q16 > 1048576 || c.ki_q16 < 0 || c.ki_q16 > 1048576 ||
        c.duty_limit_q16 < 0 || c.duty_limit_q16 > 65536 || c.integral_limit_q16 < 0 ||
        c.integral_limit_q16 > 65536 || target < -1000000 || target > 1000000 ||
        measured < -1000000 || measured > 1000000 || integral < -c.integral_limit_q16 ||
        integral > c.integral_limit_q16 || period < 1 || period > 100000) {
        return {0, 0, true};
    }
    const int64_t error = static_cast<int64_t>(target) - measured;
    const int64_t proportional = rounded(c.kp_q16 * error, 1000);
    const int32_t candidate =
        clamp(integral + rounded(c.ki_q16 * error * period, 1000000000), c.integral_limit_q16);
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
