#pragma once
#include "dmotor/registers.hpp"
#include <cstdint>

namespace dmotor {
namespace timing {
// Current control contract: 1 kHz updates and at most 100 kHz encoder events.
constexpr uint32_t control_tick_cycles = reg::FABRIC_HZ / 1000;
constexpr uint32_t encoder_min_edge_cycles = reg::FABRIC_HZ / 100000;
// Configurable timeout bounds: one control tick through ten seconds.
constexpr uint32_t zero_timeout_min_cycles = control_tick_cycles;
constexpr uint32_t zero_timeout_max_cycles = 10ULL * reg::FABRIC_HZ;
constexpr uint32_t acquisition_timeout_min_cycles = control_tick_cycles;
// Maximum measurable encoder interval, independently of the configured timeout.
constexpr uint32_t encoder_max_interval_cycles = 10ULL * reg::FABRIC_HZ;
static_assert(reg::FABRIC_HZ % 100000 == 0, "Fabric clock must represent timing units exactly");
static_assert(10ULL * reg::FABRIC_HZ <= UINT32_MAX, "Timing limits must fit uint32_t");
} // namespace timing
} // namespace dmotor
