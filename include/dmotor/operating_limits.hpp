#pragma once
#include <cstdint>
#include "dmotor/capabilities.hpp"
#include "dmotor/timing.hpp"
#include "dmotor/detail/bounded.hpp"
namespace dmotor {
namespace operating {
constexpr unsigned max_wheel_diameter_mm = 100; // Host geometry; no FPGA conversion.
constexpr int32_t max_shaft_speed_rpm = capability::maximum_speed_rpm;
constexpr int32_t millirpm_per_rpm = 1000;
constexpr int32_t max_speed_mrpm = max_shaft_speed_rpm * millirpm_per_rpm;
constexpr unsigned velocity_fractional_bits = 0;
constexpr unsigned velocity_bits = capability::velocity_bits;
constexpr unsigned velocity_error_bits = velocity_bits + 1;
constexpr int32_t velocity_scale = millirpm_per_rpm;
constexpr int64_t integral_scale = int64_t{velocity_scale} * timing::control_hz;
constexpr unsigned integral_bits = 37; // +/-65536 * 1000000 at 1 kHz.
static_assert(timing::control_hz == 1000, "Re-derive integral width for changed control rate");
// Precondition: caller has checked +/-max_speed_mrpm BEFORE conversion.
// Round nearest, ties away from zero. Public units remain milliRPM.
inline int32_t velocity_q(int32_t mrpm) {
    const int32_t magnitude = mrpm < 0 ? -mrpm : mrpm;
    const int32_t q = (magnitude * velocity_scale + millirpm_per_rpm / 2) /
                      millirpm_per_rpm;
    return static_cast<int32_t>(detail::narrow_signed<velocity_bits>(mrpm < 0 ? -q : q));
}
} // namespace operating
} // namespace dmotor
