#include "dmotor/motion.hpp"
#include "dmotor/timing.hpp"
#include <cstdlib>
#include <iostream>

static void check(bool condition) {
    if (!condition) {
        std::cerr << "position-error boundary mismatch\n";
        std::exit(1);
    }
}
static dmotor::MotionConfig config() {
    dmotor::MotionConfig c;
    c.generation = 1;
    c.max_speed_mrpm = 1000000;
    c.acceleration_mrpm_s = 10000000;
    c.deceleration_mrpm_s = 1;
    c.counts_num = 6000;
    c.counts_den = 1;
    c.settle_ticks = 1;
    c.stop_timeout_us = 2000000;
    return c;
}
static dmotor::Descriptor command(int64_t target) {
    dmotor::Descriptor d{};
    d[0] = 2;
    d[3] = d[4] = 1;
    d[5] = 1000000;
    d[6] = static_cast<uint32_t>(static_cast<uint64_t>(target));
    d[7] = static_cast<uint32_t>(static_cast<uint64_t>(target) >> 32);
    d[11] = 2000000;
    return d;
}
int main() {
    const int64_t positions[] = {INT64_MIN, -4294967296LL, -1, 0, 4294967296LL, INT64_MAX};
    const uint32_t tolerances[] = {0, 1, INT32_MAX, UINT32_MAX};
    for (auto position : positions) {
        for (auto target : positions) {
            const uint64_t distance = target >= position
                ? static_cast<uint64_t>(target) - static_cast<uint64_t>(position)
                : static_cast<uint64_t>(position) - static_cast<uint64_t>(target);
            for (auto tolerance : tolerances) {
                auto c = config();
                c.position_tolerance = tolerance;
                dmotor::MotionExecutor executor;
                const auto result = executor.step(false, true, true, command(target), false,
                    false, 1, dmotor::timing::control_tick_cycles, position, 0, true, c);
                check(!result.faults && result.accepted);
                check(result.completion_valid == (distance <= tolerance));
            }
        }
    }
    // At this deliberately low deceleration, braking distance exceeds 32 bits.
    // The ramp must decelerate exactly at the wide braking/lookahead threshold.
    constexpr uint64_t threshold = 50000000100ULL;
    const uint64_t distances[] = {UINT32_MAX, uint64_t{UINT32_MAX} + 1,
                                  threshold - 1, threshold, threshold + 1};
    for (auto distance : distances) {
        for (int direction : {-1, 1}) {
            auto c = config();
            dmotor::MotionExecutor executor;
            const int64_t target = static_cast<int64_t>(distance) * direction;
            dmotor::MotionResult result;
            for (uint64_t tick = 1; tick <= 1000; ++tick) {
                result = executor.step(false, true, tick == 1, command(target), false,
                    false, 1, tick * dmotor::timing::control_tick_cycles, 0,
                    direction * 1000000, true, c);
                check(!result.faults && result.busy);
            }
            const int32_t expected = direction * (distance <= threshold ? 999999 : 1000000);
            check(result.ramp_mrpm == expected);
        }
    }
    std::cout << "position-error: full-width endpoints, tolerance and wide braking boundaries passed\n";
}
