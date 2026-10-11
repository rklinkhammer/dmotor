#include "dmotor/controller.hpp"
#include <cstdlib>
#include <iostream>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << __LINE__ << ": " #x "\n";                                                 \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
static uint32_t config[20] = {1,     130000, 1000000, 1000000, 10296,    5,       1,   100, 2, 655,
                              65536, 65536,  65536,   1000000, 10000000, 6000000, 128, 0,   0, 0};
static uint32_t command[16], reply[24];
static uint64_t now, count, edge;
static uint32_t direction, invalid;
static bool negative;
static void call(bool reset = false, bool valid = false, bool ack = false, bool stop = false,
                 bool authorized = true, bool capture_fault = false) {
    dmotor_controller(reset, authorized, capture_fault, now, count, edge, direction, invalid,
                      negative, 0, 1, valid, command, ack, stop, config, reply);
}
static void tick(bool event = false, bool valid = false, bool ack = false, bool stop = false,
                 bool authorized = true, bool capture_fault = false) {
    now += 100000;
    if (event) {
        ++count;
        edge = now;
    }
    call(false, valid, ack, stop, authorized, capture_fault);
}
static void reset() {
    now = count = edge = 0;
    direction = invalid = 0;
    negative = false;
    for (auto& w : command) {
        w = 0;
    }
    command[0] = 1;
    command[3] = command[4] = 1;
    command[5] = 1000;
    call(true);
    call();
}
static void startup() {
    reset();
    for (unsigned i = 0; i < 99; ++i) {
        tick(false, true);
        CHECK(!(reply[2] & 8) && reply[0] == 0 && reply[17] == 2 && !reply[16]);
    }
    tick(false, true);
    CHECK((reply[2] & 12) == 12 && (reply[13] & 3) == 3 && !reply[17] && !reply[16]);
    tick(false, false, true);
}
int main() {
    startup();
    const auto ramp = reply[1];
    tick(true);
    CHECK(reply[17] == 1 && !(reply[13] & 1) && reply[0] <= 128 && reply[1] == ramp && !reply[16]);
    for (unsigned i = 0; i < 28; ++i) {
        tick();
        CHECK(reply[17] == 1 && reply[1] == ramp && !reply[16]);
    }
    tick(true);
    CHECK((reply[13] & 1) && !reply[17] && !reply[16]);
    // First reversed edge invalidates period; another qualifies it without guessing speed.
    negative = true;
    ++direction;
    tick(true);
    CHECK(reply[17] == 1 && !reply[16]);
    for (unsigned i = 0; i < 28; ++i) {
        tick();
    }
    tick(true);
    CHECK((reply[13] & 1) && dmotor::signed_word(reply[12]) < 0 && !reply[16]);
    // New commands wait through acquisition and are accepted only on qualification.
    negative = false;
    ++direction;
    tick(true, true);
    CHECK(!(reply[2] & 8));
    tick(true, true);
    CHECK((reply[2] & 8) && !reply[16]);
    // A protective decoder diagnostic cannot borrow the acquisition grace period.
    ++invalid;
    tick();
    CHECK(reply[16] & 4);
    CHECK(!reply[0]);
    tick();
    CHECK(reply[16] & 4);
    startup();
    tick(true);
    for (unsigned i = 0; i < 59; ++i) {
        tick();
    }
    CHECK(!reply[16]);
    tick(true); // measurement arrives exactly at the 60 ms deadline: expiration wins
    CHECK(reply[16] & 16);
    CHECK(!reply[0]);
    // Repeated reversal events must not extend the deadline.
    startup();
    tick(true);
    for (unsigned i = 0; i < 60; ++i) {
        ++direction;
        negative = !negative;
        tick(true);
    }
    CHECK(reply[16] & 16);
    // Controlled stop during acquisition immediately removes held drive, cannot settle on invalid speed.
    startup();
    tick(true);
    tick(false, false, false, true);
    CHECK(reply[0] == 0 && reply[17] == 1 && !(reply[2] & 1));
    tick(true);
    CHECK(!reply[16]);
    // HALT/watchdog authorization loss wins even with command and edge together.
    startup();
    tick(true, true, false, false, false);
    CHECK(!reply[0] && !(reply[2] & 8));
    startup();
    tick(false, false, false, false, true, true);
    CHECK((reply[16] & 2) && !reply[0]);
    reset();
    config[15] = 10000000;
    tick();
    CHECK(reply[16] & 1);
    config[15] = 6000000;
    reset();
    now += 100000;
    call();
    config[16] = 129;
    tick();
    CHECK(reply[16] & 1);
    config[16] = 128;
    std::cout << "controller: warmup, bounded acquisition, startup/reversal, stop, deadline "
                 "priority and protective faults passed\n";
}
