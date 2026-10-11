#include "dmotor/command_engine.hpp"
#include "dmotor/ring_config.hpp"
#include <cstdlib>
#include <iostream>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << __LINE__ << ": " #x "\n";                                                 \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
static uint32_t reply[16];
static uint64_t timestamp = 0;
static bool ready = false, cancel = false, complete = false, reset = false, drained = true;
static uint32_t finish_epoch = 0, finish_sequence = 0;
static void call(uint32_t op = 0, uint32_t a = 0, uint32_t b = 0, uint32_t c = 0) {
    uint32_t request[4] = {op, a, b, c};
    dmotor_command_engine(reset, request, ++timestamp, ready, 0, 130000, drained, 7, cancel, 8,
                          complete, finish_epoch, finish_sequence, 0, reply);
}
static void descriptor(uint32_t sequence, uint32_t epoch, bool bad = false) {
    for (uint32_t i = 0; i < 16; ++i) {
        uint32_t value = 0;
        if (i == 0) {
            value = 1;
        }
        if (i == 2) {
            value = sequence;
        }
        if (i == 3) {
            value = epoch;
        }
        if (i == 4) {
            value = 7;
        }
        if (i == 5) {
            value = 1000;
        }
        if (i == 12 && bad) {
            value = 1;
        }
        call(1, sequence & dmotor::ring_mask, i, value);
        CHECK(reply[0]);
    }
}
int main() {
    reset = true;
    call();
    reset = false;
    CHECK(reply[1] == 0 && reply[5] == 0);
    drained = false;
    call(3);
    CHECK(!reply[0]);
    drained = true;
    call(3);
    CHECK(reply[0] && reply[5] == 1);
    descriptor(0, 1);
    call(2, 1, 2);
    CHECK(!reply[0]);
    call(2, 1, 1);
    CHECK(reply[0]);
    call(1, 0, 5, 2000);
    CHECK(!reply[0]);
    call();
    CHECK(!reply[6]);
    ready = true;
    call();
    CHECK(reply[6] && (reply[1] & 1));
    call(8, 5);
    CHECK(reply[7] == 1000);
    descriptor(1, 1);
    call(2, 2, 1);
    CHECK(reply[0]);
    complete = true;
    finish_epoch = 99;
    finish_sequence = 0;
    call();
    CHECK(reply[4] == 0 && (reply[1] & 1));
    finish_epoch = 1;
    cancel = true;
    call();
    CHECK((reply[1] & 4) && !(reply[1] & 1));
    CHECK(reply[4] == 1 && reply[9] == 1 && reply[11] == 1 && reply[12] == 2);
    cancel = false;
    complete = false;
    ready = false;
    call(7, 0, 2);
    CHECK(reply[0] && reply[7] == 2); // active aborted
    call(3);
    CHECK(!reply[0]);
    call(5, 1);
    CHECK(reply[0]);
    call(6);
    call(3);
    CHECK(reply[5] == 2);
    // Fill completion ring by rejecting malformed descriptors; all configured slots usable.
    for (uint32_t i = 0; i < dmotor::ring_depth; ++i) {
        ready = false;
        descriptor(i, 2, true);
        call(2, i + 1, 2);
        CHECK(reply[0]);
        ready = true;
        call();
        CHECK(!reply[6] && reply[4] == i + 1);
    }
    ready = false;
    descriptor(dmotor::ring_depth, 2);
    call(2, dmotor::ring_depth + 1, 2);
    ready = true;
    call();
    CHECK(!reply[6] && reply[3] == dmotor::ring_depth); // capacity reservation before capture
    call(5, dmotor::ring_depth + 1);
    CHECK(!reply[0]);
    call(5, dmotor::ring_depth);
    CHECK(reply[0]);
    call();
    CHECK(reply[6]);
    // Stale completion cannot finish current command even within the same epoch.
    complete = true;
    finish_epoch = 2;
    finish_sequence = 0;
    call();
    CHECK(reply[1] & 1);
    finish_sequence = dmotor::ring_depth;
    call();
    CHECK(!(reply[1] & 1) && reply[4] == dmotor::ring_depth + 1);
    complete = false;
    call(4);
    CHECK(!(reply[1] & 2));
    ready = false;
    call(5, dmotor::ring_depth + 1);
    call(3);
    CHECK(reply[5] == 3);
    descriptor(0, 3);
    call(2, 1, 3);
    call(4);
    call(3);
    CHECK(!reply[0]); // cannot discard queued work
    cancel = true;
    call();
    cancel = false;
    call(6);
    call(3);
    CHECK(reply[5] == 4);
    call(99);
    CHECK(!reply[0]);
    std::cout << "command engine: publication, activation, stale completion, cancellation, full "
                 "ring passed\n";
}
