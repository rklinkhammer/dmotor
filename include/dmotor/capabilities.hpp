#pragma once
#include <cstdint>
namespace dmotor {
namespace capability {
// Controller envelope; each motor keeps an independently configured lower limit.
constexpr int32_t maximum_speed_rpm = 400;
constexpr int32_t millirpm_per_rpm = 1000;
constexpr int32_t maximum_speed_mrpm = maximum_speed_rpm * millirpm_per_rpm;
constexpr unsigned velocity_bits = 20;
constexpr unsigned velocity_error_bits = 21;
constexpr unsigned pi_numerator_bits = 40;
constexpr unsigned pi_denominator_bits = 20;
constexpr int64_t maximum_gain_q16 = 1048576;
constexpr uint64_t maximum_pi_numerator =
    uint64_t{2} * maximum_speed_mrpm * maximum_gain_q16 + 500;
static_assert(maximum_speed_mrpm < (int64_t{1} << (velocity_bits - 1)), "velocity width");
static_assert(2LL * maximum_speed_mrpm < (int64_t{1} << (velocity_error_bits - 1)), "error width");
static_assert(maximum_pi_numerator < (uint64_t{1} << pi_numerator_bits), "PI dividend width");
static_assert(1000000 < (uint32_t{1} << pi_denominator_bits), "PI divisor width");
} // namespace capability
} // namespace dmotor
