#pragma once
#include "dmotor/timing.hpp"
namespace test_timing {
constexpr uint32_t tick_cycles = dmotor::timing::control_tick_cycles;
constexpr uint64_t cycles_us(uint64_t us) {
    return us * dmotor::timing::clocks_per_us;
}
} // namespace test_timing
