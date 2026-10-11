#include "dmotor/control.hpp"
#include <cstdlib>
#include <iostream>

// Original wide implementation is the independent numerical/fault oracle.
static dmotor::PiResult reference(dmotor::PiConfig c, int32_t target, int32_t measured,
                                  int32_t integral, uint32_t period) {
    if (c.kp_q16 < 0 || c.kp_q16 > 1048576 || c.ki_q16 < 0 || c.ki_q16 > 1048576 ||
        c.duty_limit_q16 < 0 || c.duty_limit_q16 > 65536 || c.integral_limit_q16 < 0 ||
        c.integral_limit_q16 > 65536 || target < -1000000 || target > 1000000 ||
        measured < -1000000 || measured > 1000000 || integral < -c.integral_limit_q16 ||
        integral > c.integral_limit_q16 || period < 1 || period > 100000) {
        return {0, 0, true};
    }
    const auto round = [](int64_t value, int64_t divisor) {
        return value < 0 ? -((-value + divisor / 2) / divisor) : (value + divisor / 2) / divisor;
    };
    const auto clamp = [](int64_t value, int32_t limit) {
        return value > limit ? limit : value < -limit ? -limit : static_cast<int32_t>(value);
    };
    const int64_t error = static_cast<int64_t>(target) - measured;
    const int64_t proportional = round(c.kp_q16 * error, 1000);
    const int32_t candidate = clamp(integral + round(c.ki_q16 * error * period, 1000000000),
                                    c.integral_limit_q16);
    const int64_t effort = proportional + candidate;
    if ((effort <= c.duty_limit_q16 && effort >= -c.duty_limit_q16) ||
        (effort > c.duty_limit_q16 && error < 0) ||
        (effort < -c.duty_limit_q16 && error > 0)) {
        integral = candidate;
    }
    return {clamp(proportional + integral, c.duty_limit_q16), integral, false};
}
static void check(dmotor::PiConfig c, int32_t target, int32_t measured, int32_t integral,
                  uint32_t period) {
    const auto expected = reference(c, target, measured, integral, period);
    int32_t duty_out, integral_out;
    bool fault;
    dmotor_pi(c.kp_q16, c.ki_q16, c.duty_limit_q16, c.integral_limit_q16, target,
              measured, integral, period, duty_out, integral_out, fault);
    if (duty_out != expected.duty_q16 || integral_out != expected.integral_q16 ||
        fault != expected.range_fault) {
        std::cerr << "PI differs from original wide implementation\n";
        std::exit(6);
    }
}

int main() {
    int32_t duty, integral;
    bool fault;
    dmotor_pi(65536, 65536, 65536, 65536, 1000, 0, 0, 1000, duty, integral, fault);
    if (fault || duty != 65536 || integral != 0) {
        return 1; // anti-windup at saturation
    }
    dmotor_pi(655, 65536, 65536, 65536, 1000, 0, 0, 1000, duty, integral, fault);
    if (fault || duty != 721 || integral != 66) {
        return 2;
    }
    dmotor_pi(655, 65536, 65536, 65536, -1000, 0, 0, 1000, duty, integral, fault);
    if (fault || duty != -721 || integral != -66) {
        return 3;
    }
    dmotor_pi(655, 65536, 65536, 65536, INT32_MIN, 0, 0, 1000, duty, integral, fault);
    if (!fault || duty != 0) {
        return 4;
    }
    dmotor_pi(1048576, 1048576, 65536, 65536, 1000000, -1000000, 0, 100000, duty, integral, fault);
    if (fault || duty != 65536 || integral != 0) {
        return 5;
    }
    for (int32_t speed : {-1000000, -1, 0, 1, 1000000}) {
        for (int32_t measured : {-1000000, 0, 1000000}) {
            for (uint32_t period : {1U, 1000U, 100000U}) {
                for (int32_t integral : {-65536, 0, 65536}) {
                    check({1048576, 1048576, 65536, 65536}, speed, measured, integral, period);
                }
            }
        }
    }
    // Half-unit boundaries for both rounded divisions, both signs.
    for (int32_t value : {499, 500, 501, -499, -500, -501}) {
        check({1, 0, 65536, 65536}, value, 0, 0, 1);
        check({0, 1000, 65536, 65536}, value, 0, 0, 1000);
    }
    uint32_t seed = 0x7a54be12U;
    const auto random = [&seed]() { seed = seed * 1664525U + 1013904223U; return seed; };
    for (unsigned n = 0; n < 2048; ++n) {
        dmotor::PiConfig c{static_cast<int32_t>(random() % 1048577),
                           static_cast<int32_t>(random() % 1048577),
                           static_cast<int32_t>(random() % 65537),
                           static_cast<int32_t>(random() % 65537)};
        const int32_t target = static_cast<int32_t>(random() % 2000001) - 1000000;
        const int32_t measured = static_cast<int32_t>(random() % 2000001) - 1000000;
        const int32_t integral = static_cast<int32_t>(random() % (2 * c.integral_limit_q16 + 1)) -
                                 c.integral_limit_q16;
        check(c, target, measured, integral, random() % 100000 + 1);
    }
    // Reject invalid inputs before masking, including values that alias valid ones.
    for (int32_t value : {INT32_MIN, -1, 1048577, 2097153, INT32_MAX}) {
        check({value, 0, 65536, 65536}, 0, 0, 0, 1000);
        check({0, value, 65536, 65536}, 0, 0, 0, 1000);
        check({0, 0, value, 65536}, 0, 0, 0, 1000);
        check({0, 0, 65536, value}, 0, 0, 0, 1000);
        check({0, 0, 65536, 65536}, value, 0, 0, 1000);
        check({0, 0, 65536, 65536}, 0, value, 0, 1000);
        check({0, 0, 65536, 65536}, 0, 0, value, 1000);
    }
    for (uint32_t period : {0U, 100001U, 131073U, UINT32_MAX}) {
        check({1, 1, 65536, 65536}, 1000, 0, 0, period);
    }
    check({0, 0, 0, 0}, 0, 0, 0, 1);
    std::cout << "PI: signed rounding, limits, anti-windup, range checks passed\n";
}
