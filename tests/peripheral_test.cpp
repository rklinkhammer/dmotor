#include "dmotor/peripheral.hpp"
#include "dmotor/registers.hpp"
#include <cstdlib>
#include <iostream>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << __LINE__ << ": " #x "\n";                                                 \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
using namespace dmotor::reg;
static uint32_t result, status, epoch, seq;
static bool error, activated, disable, stop, ready = false, cancel = false, reset = false;
static uint64_t now = 0, timeout = 0;
static uint32_t safety_state = 0, action = 0, argument = 0, refresh_session = 0, expected = 1;
static bool configured = false;
static void access(bool valid, bool wr, uint32_t address = 0, uint32_t data = 0,
                   uint8_t strobes = 15) {
    dmotor_peripheral(reset, valid, wr, address, data, strobes, ++now, ready, 0, 130000, true, 7,
                      cancel, 8, false, 0, 0, 0, result, error, status, activated, epoch, seq,
                      disable, stop, safety_state, 0, 0, expected, action, argument,
                      refresh_session, timeout, configured);
}
static void write(uint32_t a, uint32_t d) {
    access(true, true, a, d);
    CHECK(!error);
}
int main() {
    reset = true;
    access(false, false);
    reset = false;
    access(true, false, IDENTITY);
    CHECK(result == 0x444d4f54);
    access(true, false, CLOCK_HZ);
    CHECK(result == 100000000);
    access(true, true, REOPEN, 1, 1);
    CHECK(error);
    access(true, true, REOPEN + 1, 1);
    CHECK(error);
    write(WATCHDOG_US, 1000);
    write(CONFIG_COMMIT, 1);
    write(REOPEN, 1);
    CHECK(epoch == 1);
    for (unsigned i = 0; i < 16; ++i) {
        uint32_t value = i == 0 ? 1 : i == 3 ? 1 : i == 4 ? 1 : i == 5 ? 1000 : 0;
        write(SUBMISSION_OFFSET + 4 * i, value);
    }
    write(PUBLISH_SESSION, 1);
    write(PUBLISH, 1);
    access(true, true, SUBMISSION_OFFSET + 20, 2);
    CHECK(error);
    access(false, false);
    CHECK(!activated);
    write(ENABLE, 1);
    CHECK(action == 1);
    safety_state = 1;
    access(true, true, WATCHDOG_US, 2000);
    CHECK(error);
    write(REFRESH_SESSION, 1);
    write(REFRESH_SEQUENCE, 1);
    CHECK(action == 2 && argument == 1);
    expected = 2;
    access(true, true, REFRESH_SEQUENCE, 1);
    CHECK(error);
    CHECK(timeout == 100000 && configured);
    ready = true;
    access(false, false);
    CHECK(activated && seq == 0);
    access(true, false, ACTIVE_OFFSET + 20);
    CHECK(result == 1000);
    cancel = true;
    access(true, true, PUBLISH_SESSION, 99);
    CHECK(error && (status & 4));
    cancel = false;
    safety_state = 0;
    access(true, false, PUBLISH_SESSION);
    CHECK(result == 1);
    access(true, false, COMPLETION_OFFSET + 8);
    CHECK(result == 2);
    write(COMPLETION_ACK, 1);
    write(CANCEL_ACK, 1);
    ready = false;
    write(REOPEN, 1);
    CHECK(epoch == 2);
    write(STOP, 1);
    CHECK(stop && !disable && (status & 4));
    write(CANCEL_ACK, 1);
    write(REOPEN, 1);
    write(DISABLE, 1);
    CHECK(disable && !stop);
    access(true, true, ENABLE, 1);
    CHECK(error); // Not pretending motion integration exists.
    access(true, false, 0xdead0000);
    CHECK(!error && result == 0);
    access(true, true, 0xdead0000, 0);
    CHECK(error);
    // Exercise physical-slot decoding after the completion counter wraps.
    write(CANCEL_ACK, 1);
    ready = false;
    write(REOPEN, 1);
    const uint32_t session = epoch;
    write(PUBLISH_SESSION, session);
    ready = true;
    safety_state = 1;
    for (uint32_t counter = 0; counter < COMPLETION_SLOTS + 2; ++counter) {
        const uint32_t slot = counter & dmotor::ring_mask;
        for (uint32_t word = 0; word < SUBMISSION_STRIDE / 4; ++word) {
            // Invalid opcode produces a rejection completion without motion.
            const uint32_t value = word == 2 ? counter : word == 3 ? session : 0;
            write(SUBMISSION_OFFSET + slot * SUBMISSION_STRIDE + word * 4, value);
        }
        write(PUBLISH, counter + 1);
        access(false, false);
        access(true, false, COMPLETION_OFFSET + slot * COMPLETION_STRIDE);
        CHECK(result == counter);
        access(true, false, COMPLETION_OFFSET + slot * COMPLETION_STRIDE + 8);
        CHECK(result == 1);
        write(COMPLETION_ACK, counter + 1);
    }
    std::cout
        << "peripheral: decode, strobe/alignment errors, queue, cancellation, actions passed\n";
}
