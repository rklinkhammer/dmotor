#include "timing_fixture.hpp"
#include "dmotor/motion.hpp"
#include <cstdlib>
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
    reset();
    command[0] = 1;
    command[5] = 10000;
    tick(true);
    CHECK(reply[7] == 0 && reply[1] == 1000 && (reply[2] & 12) == 12 && reply[5] == 0);
    for (int i = 0; i < 9; ++i) {
        tick();
    }
    CHECK(reply[1] == 10000 && (reply[2] & 4)); // control continues with completion held
    tick(false, 0, 0, true, true);
    CHECK(reply[2] & 1);
    command[5] = static_cast<uint32_t>(-10000);
    tick(true);
    CHECK(dmotor::signed_word(reply[1]) == 9000);
    for (int i = 0; i < 9; ++i) {
        tick();
    }
    CHECK(reply[1] == 0);
    tick();
    CHECK(dmotor::signed_word(reply[1]) == -1000);
    tick(false, 0, 0, true, false, false, false);
    CHECK(reply[0] == 0 && reply[1] == 0);
    reset();
    command[0] = 4;
    command[8] = 32000;
    tick(true, 0, 0, false);
    CHECK(reply[0] == 32000);
    tick(false, 0, 0, false, true);
    command[0] = 5;
    command[8] = 0;
    command[9] = 2000;
    tick(true, 0, 0, false);
    CHECK((reply[2] & 2) && reply[0] == 32000);
    tick(false, 0, 0, false);
    CHECK(reply[2] & 2);
    tick(false, 0, 0, false);
    CHECK((reply[2] & 4) && !(reply[2] & 2) && reply[0] == 32000);
    // Relative move resolves from measured position at activation, then coasts after settling.
    reset();
    command[0] = 3;
    command[5] = 10000;
    command[6] = 10;
    command[11] = 100000;
    tick(true, 100);
    CHECK(reply[2] & 2);
    tick(false, 110);
    tick(false, 110);
    CHECK(reply[7] == 0 && (reply[2] & 4) && reply[0] == 0);
    tick(false, 120);
    CHECK(reply[0] == 0); // drift must not restart holding
    // Timeout wins a would-be settled position command.
    reset();
    command[0] = 2;
    command[5] = 1000;
    command[6] = 0;
    command[11] = 1000;
    tick(true);
    tick();
    CHECK(reply[7] & 4);
    CHECK(reply[0] == 0 && reply[5] == 2);
    reset();
    command[0] = 6;
    command[11] = 100000;
    tick(true);
    tick();
    CHECK(reply[5] == 0 && (reply[2] & 4));
    reset();
    command[0] = 4;
    command[8] = 32000;
    tick(true, 0, 0, false);
    tick(false, 0, 0, true, true);
    tick(false, 0, 5000, true, false, true);
    CHECK(reply[1] == 4000);
    for (int i = 0; i < 6; ++i) {
        tick();
    }
    CHECK(reply[0] == 0 && (reply[2] & 1));
    reset();
    command[0] = 1;
    command[5] = 1000;
    tick(true, 0, 0, false);
    CHECK(reply[7] & 8);
    CHECK(reply[0] == 0 && reply[5] == 2 && (reply[2] & 4));
    // Closed-loop takeover starts from measured speed, not stale open-loop state.
    reset();
    command[0] = 4;
    command[8] = 32000;
    tick(true, 0, 5000);
    tick(false, 0, 5000, true, true);
    command[0] = 1;
    command[8] = 0;
    command[5] = 10000;
    tick(true, 0, 5000);
    CHECK(reply[1] == 6000 && !reply[7]);
    // Fractional integral: 0.001 duty LSB per tick is retained.
    reset();
    config[9] = 0;
    config[10] = 1;
    command[0] = 1;
    command[5] = 1000;
    for (int i = 0; i < 1000; ++i) {
        tick(i == 0);
    }
    CHECK(reply[0] == 1 && !reply[7]);
    config[9] = 655;
    config[10] = 65536;
    // Fractional acceleration: 1 milli-RPM/s advances exactly 1 milli-RPM in a second.
    reset();
    config[2] = 1;
    command[0] = 1;
    command[5] = 1000;
    for (int i = 0; i < 1000; ++i) {
        tick(i == 0);
    }
    CHECK(reply[1] == 1);
    config[2] = 1000000;
    reset();
    command[0] = 1;
    command[5] = 1000;
    tick(true);
    time_cycles += test_timing::tick_cycles;
    tick();
    CHECK(reply[7] & 2);
    reset();
    command[0] = 3;
    command[5] = 1000;
    command[6] = 1;
    command[11] = 100000;
    tick(true, INT64_MAX);
    CHECK(reply[5] == 1 && reply[6] == static_cast<uint32_t>(dmotor::Error::overflow));
    CHECK(!reply[7]);
    // Compare the HLS top against the original wide quotient/remainder ramp.
    // Exercise fractional carries, rate changes, reversal, clamping and reset.
    const uint32_t rates[] = {1, 999, 1000, 1001, 9999999, 10000000};
    for (unsigned run = 0; run < 2; ++run) {
        reset();
        int32_t reference = 0;
        uint64_t remainder = 0;
        int32_t target = 1000;
        for (unsigned i = 0; i < 300; ++i) {
            config[2] = rates[(i / 25 + run) % 6];
            config[3] = rates[(i / 25 + run + 2) % 6];
            const bool publish = i % 50 == 0;
            if (publish) {
                target = (i / 50) % 2 ? -1000 : 1000;
                command[0] = 1;
                command[5] = static_cast<uint32_t>(target);
            }
            int32_t goal = target;
            if ((goal < 0 && reference > 0) || (goal > 0 && reference < 0)) {
                goal = 0;
            }
            const uint32_t rate = std::abs(goal) < std::abs(reference) ? config[3] : config[2];
            remainder += rate;
            const int64_t advance = remainder / 1000;
            remainder %= 1000;
            const int32_t change = goal - reference;
            if (std::abs(change) <= advance) {
                reference = goal;
                remainder = 0;
            } else {
                reference += change > 0 ? advance : -advance;
            }
            tick(publish, 0, 0, true, true);
            CHECK(!reply[7] && dmotor::signed_word(reply[1]) == reference);
        }
    }
    // Width boundaries: both signed endpoints must survive retained state.
    config[1] = 1000000;
    config[9] = 0;
    config[10] = 1048576;
    for (int sign : {-1, 1}) {
        reset();
        command[0] = 1;
        command[5] = static_cast<uint32_t>(sign * 1000000);
        tick(true, 0, sign * 1000000);
        CHECK(!reply[7] && dmotor::signed_word(reply[1]) == sign * 1000000);
        tick(false, 0, -sign * 1000000);
        CHECK(!reply[7] && dmotor::signed_word(reply[0]) == sign * 65536);
        // Zero error retains the saturated integral across the next call.
        tick(false, 0, sign * 1000000);
        CHECK(dmotor::signed_word(reply[0]) == sign * 65536);
        reset();
        command[0] = 4;
        command[8] = static_cast<uint32_t>(sign * 65536);
        tick(true, 0, 0, false);
        tick(false, 0, 0, false);
        CHECK(!reply[7] && dmotor::signed_word(reply[0]) == sign * 65536);
    }
    // Full-width invalid input must be rejected before any narrowing.
    reset();
    command[0] = 9; // Would alias velocity opcode 1 if narrowed prematurely.
    command[5] = 1000;
    tick(true);
    CHECK(reply[5] == 1 && reply[0] == 0);
    reset();
    command[0] = 1;
    command[5] = 2098152; // Would alias 1000 in signed 21-bit storage.
    tick(true);
    CHECK(reply[5] == 1 && reply[0] == 0);
    config[2] = config[3] = 1000000;
    // Preparation must reject full-width values before masking any operand.
    const unsigned fields[] = {1, 3, 4, 5, 12};
    const uint32_t invalid_values[] = {1049576, 16777217, 1048577, 1048577, 131073};
    for (unsigned i = 0; i < 5; ++i) {
        const auto saved = config[fields[i]];
        config[fields[i]] = invalid_values[i];
        reset();
        command[0] = 1;
        command[5] = 1000;
        tick(true);
        CHECK((reply[7] & 1) && reply[0] == 0 && !(reply[2] & 8));
        config[fields[i]] = saved;
    }
    // Exact CPR/speed cross-product boundary, and maximum denominator/rate.
    config[4] = 1000000;
    for (uint32_t den : {166U, 167U, 1000000U}) {
        config[5] = den;
        config[3] = 10000000;
        reset();
        command[0] = 2;
        command[5] = 1000;
        command[6] = 10000;
        command[11] = 1000000;
        tick(true);
        CHECK(bool(reply[7] & 1) == (den == 166));
        if (den != 166) {
            CHECK(!reply[7] && (reply[2] & 2));
        }
    }
    std::cout << "motion: six commands, ramps, residuals, settling, timeouts, inhibit and timing "
                 "passed\n";
}
