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
    time_cycles += 100000;
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
    time_cycles += 100000;
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
    std::cout << "motion: six commands, ramps, residuals, settling, timeouts, inhibit and timing "
                 "passed\n";
}
