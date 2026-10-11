#pragma once
#include <cstdint>
namespace dmotor {
struct PiConfig {
    int32_t kp_q16; // normalized duty / RPM, scaled by 65536
    int32_t ki_q16; // normalized duty / (RPM second), scaled by 65536
    int32_t duty_limit_q16;
    int32_t integral_limit_q16;
};
struct PiResult {
    int32_t duty_q16;
    int32_t integral_q16;
    bool range_fault;
};
PiResult pi_step(PiConfig config, int32_t target_mrpm, int32_t measured_mrpm, int32_t integral_q16,
                 uint32_t period_us);
} // namespace dmotor
extern "C" void dmotor_pi(int32_t kp_q16, int32_t ki_q16, int32_t limit_q16,
                          int32_t integral_limit_q16, int32_t target_mrpm, int32_t measured_mrpm,
                          int32_t integral_q16, uint32_t period_us, int32_t& duty_out,
                          int32_t& integral_out, bool& range_fault);
