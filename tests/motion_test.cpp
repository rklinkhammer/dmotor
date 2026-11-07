#include "timing_fixture.hpp"
#include "dmotor/motion.hpp"
#include <cstdlib>
#include <cmath>
#include "dmotor/operating_limits.hpp"
#include <iostream>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << __LINE__ << ": " #x "\n";                                                 \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
static uint32_t config[16] = {1, 130000, 1000000, 1000000, 10296, 5,       1, 100,
                              2, 655,    65536,   65536,   65536, 1000000, 0, 0};
static uint32_t command[16] = {}, reply[16] = {};
static uint64_t time_cycles = 0;
static void tick(bool valid = false, int64_t position = 0, int32_t speed = 0, bool feedback = true,
                 bool ack = false, bool stop = false, bool authorized = true) {
    time_cycles += test_timing::tick_cycles;
    dmotor_motion(false, authorized, valid, command, config, ack, stop, 1, time_cycles, position,
                  speed, feedback, reply);
#ifdef DMOTOR_TRACE
    for (auto word : reply) std::cout << word << ',';
    std::cout << '\n';
#endif
}
static void reset() {
    time_cycles = 0;
    dmotor_motion(true, false, false, command, config, false, false, 1, 0, 0, 0, false, reply);
    for (auto& word : command) {
        word = 0;
    }
    command[3] = 1;
    command[4] = 1;
}
int main() {
    // Opposite-sign feedback exercises the maximum PI product/divider input.
    for (int sign : {-1, 1}) {
        config[1] = dmotor::capability::maximum_speed_mrpm;
        config[2] = 10000000; config[3] = 10000000;
        config[9] = 1048576; config[10] = 1048576;
        config[11] = 65536; config[12] = 65536;
        reset(); command[0] = 1;
        command[5] = static_cast<uint32_t>(sign * dmotor::capability::maximum_speed_mrpm);
        for (unsigned i=0; i<85; ++i) {
            tick(i==0, 0, -sign * dmotor::capability::maximum_speed_mrpm);
            CHECK(!reply[7]);
            CHECK(dmotor::signed_word(reply[0]) == sign * 65536);
        }
        // Non-saturating independently computed output at the same error.
        config[9] = 1; config[10] = 0;
        reset(); command[0] = 1;
        command[5] = static_cast<uint32_t>(sign * dmotor::capability::maximum_speed_mrpm);
        for(unsigned i=0;i<85;++i)
            tick(i==0, 0, -sign * dmotor::capability::maximum_speed_mrpm);
        CHECK(!reply[7] && dmotor::signed_word(reply[0]) == sign * 800);
    }
    config[2]=1000000; config[3]=1000000; config[9]=655; config[10]=65536;
    // Exhaust every accepted milliRPM value against an independent FP oracle.
    for (int32_t v = -400000; v <= 400000; ++v) {
        const auto q = dmotor::operating::velocity_q(v);
        CHECK(q == static_cast<int32_t>(std::round(double(v) * 1000.0 / 1000.0)));
        CHECK(std::abs(double(q) / 1000.0 - double(v) / 1000.0) <= 0.0);
    }
    // Compare unsaturated PI output to independent fixed-point arithmetic,
    // including fractional inputs, both directions and integral accumulation.
    for (int sign : {-1, 1}) {
        reset();
        config[1] = 400000;
        config[9] = 123;
        config[10] = 456;
        command[0] = 1;
        command[5] = static_cast<uint32_t>(sign * 1000);
        int64_t integral = 0;
        for (unsigned i = 0; i < 300; ++i) {
            const int32_t feedback = sign * (17 + static_cast<int32_t>(i % 19));
            tick(i == 0, 0, feedback);
            const auto reference_q = [](int32_t value) {
                return static_cast<int64_t>(std::round(double(value) * 1000.0 / 1000.0));
            };
            const int64_t error = reference_q(dmotor::signed_word(reply[1])) - reference_q(feedback);
            integral += 456 * error;
            const int64_t expected = static_cast<int64_t>(std::round(double(123 * error) / 1000.0)) +
                                     static_cast<int64_t>(std::round(double(integral) / 1000000.0));
            CHECK(!reply[7] && dmotor::signed_word(reply[0]) == expected);
        }
    }
    // Boundary configurations and out-of-range feedback must fault before casts.
    for (int32_t bad : {400001, -400001, INT32_MAX, INT32_MIN}) {
        reset();
        command[0] = 1;
        command[5] = 1000;
        tick(true, 0, bad);
        CHECK((reply[7] & 8) && reply[0] == 0);
    }
    for (int sign : {-1, 1}) {
        reset();
        command[0] = 1;
        command[5] = static_cast<uint32_t>(sign * 400000);
        tick(true, 0, sign * 400000);
        CHECK(!reply[7] && dmotor::signed_word(reply[1]) == sign * 400000);
    }
    reset();
    config[1] = 400001;
    tick();
    CHECK((reply[7] & 1) && reply[0] == 0);
    config[1] = 130000;
    config[9] = 655;
    config[10] = 65536;

    reset();
    command[0] = 1;
    command[5] = 10000;
    tick(true);
    CHECK(reply[1] == 1000 && !reply[7]);
    for (unsigned i = 0; i < 9; ++i) tick();
    CHECK(reply[1] == 10000);
    command[5] = static_cast<uint32_t>(-10000);
    tick(true, 0, 0, true, true);
    for (unsigned i = 0; i < 19; ++i) tick();
    CHECK(dmotor::signed_word(reply[1]) == -10000);
    command[0] = 6;
    command[5] = 0;
    command[11] = 100000;
    tick(true, 0, 0, true, true);
    for (unsigned i = 0; i < 11; ++i) tick();
    CHECK((reply[2] & 4) && !(reply[2] & 2) && reply[0] == 0 && !reply[7]);
    // Fractional ramp, signed speed error, saturation and anti-windup traces.
    for (int sign : {-1, 1}) {
        reset();
        config[2] = 1001;
        config[3] = 999;
        config[9] = config[10] = 1048576;
        command[0] = 1;
        command[5] = static_cast<uint32_t>(sign * 1000);
        for (unsigned i = 0; i < 1100; ++i) {
            tick(i == 0, 0, i == 0 ? 0 : i % 3 ? sign * 130000 : -sign * 130000);
            CHECK(!reply[7]);
        }
        CHECK(dmotor::signed_word(reply[1]) == sign * 1000);
        tick(false, 0, 0, true, false, true);
        for (unsigned i = 0; i < 1100; ++i) tick();
        CHECK(!reply[7] && reply[0] == 0 && reply[1] == 0);
    }
    config[2] = config[3] = 1000000;
    config[9] = 655;
    config[10] = 65536;
    // Timeout must beat settling, including an unsigned timestamp wrap.
    reset();
    time_cycles = UINT64_MAX - test_timing::tick_cycles;
    config[8] = 100;
    command[0] = 6;
    command[11] = 2000;
    tick(true);
    tick();
    tick();
    CHECK((reply[7] & 4) && reply[0] == 0);
    config[8] = 2;
    // Authorization loss and invalid feedback inhibit regulation.
    reset();
    command[0] = 1;
    command[5] = 1000;
    tick(true);
    tick(false, 0, 0, true, false, false, false);
    CHECK(reply[0] == 0 && reply[1] == 0);
    reset();
    command[0] = 1;
    command[5] = 1000;
    tick(true);
    tick(false, 0, 0, false);
    CHECK((reply[7] & 8) && reply[0] == 0);
#ifndef DMOTOR_TRACE
    for (uint32_t opcode : {2U, 3U, 4U, 5U}) {
        reset();
        command[0] = opcode;
        tick(true);
        CHECK(reply[5] == 1 && reply[6] == static_cast<uint32_t>(dmotor::Error::opcode));
        CHECK(reply[0] == 0);
    }
#endif
    std::cout << "velocity-only motion: ramps, PI, controlled stop, timeout and inhibition passed\n";
}
