#include "dmotor/dispatcher.hpp"
#include <cstdlib>
#include <iostream>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << __LINE__ << ": " #x "\n";                                                 \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
static uint32_t cfg[20] = {1,     130000, 1000000, 1000000, 10296,    5,       1,   100, 2, 655,
                           65536, 65536,  65536,   1000000, 10000000, 6000000, 128, 0,   0, 0};
static uint32_t reply[40];
static uint64_t now = 0, count = 0, edge = 0;
static bool authorized = false;
static int64_t position = 0;
static void service(uint32_t op = 0, uint32_t a = 0, uint32_t b = 0, uint32_t c = 0,
                    bool tick = false, bool cancel = false, bool stop = false, bool fault = false,
                    bool reset = false) {
    uint32_t request[4] = {op, a, b, c};
    dmotor_dispatcher(reset, request, tick, authorized, cancel, 8, stop, fault, true, now, count,
                      edge, 0, 0, false, position, cfg, reply);
}
static void tick(bool cancel = false, bool stop = false) {
    now += 100000;
    service(0, 0, 0, 0, true, cancel, stop);
}
static void enqueue(uint32_t opcode, uint32_t value, bool malformed = false) {
    const uint32_t sequence = reply[2], epoch = reply[5];
    for (uint32_t i = 0; i < 16; ++i) {
        uint32_t word = 0;
        if (i == 0) {
            word = opcode;
        }
        if (i == 2) {
            word = sequence;
        }
        if (i == 3) {
            word = epoch;
        }
        if (i == 4) {
            word = 1;
        }
        if (i == 5 && opcode == 1) {
            word = value;
        }
        if (i == 9 && opcode == 5) {
            word = value;
        }
        if (i == 11 && opcode == 6) {
            word = value;
        }
        if (i == 12 && malformed) {
            word = 1;
        }
        service(1, sequence & dmotor::ring_mask, i, word);
        CHECK(reply[0]);
    }
    service(2, sequence + 1, epoch);
    CHECK(reply[0]);
}
static void reset() {
    now = count = edge = 0;
    position = 0;
    authorized = false;
    service(0, 0, 0, 0, false, false, false, false, true);
    service(3);
    CHECK(reply[0] && reply[5] == 1);
    tick();
    authorized = true;
}
int main() {
    reset();
    enqueue(1, 1000);
    for (unsigned i = 0; i < 99; ++i) {
        tick();
        CHECK(reply[3] == 0 && !reply[6] && !reply[16]);
    }
    tick();
    CHECK(reply[3] == 1 && reply[4] == 1 && reply[6] && !reply[32]);
    const uint64_t activated = now;
    service(7, 0, 4);
    CHECK(reply[7] == activated);
    service(7, 0, 6);
    CHECK(reply[7] == activated);
    enqueue(5, 3000);
    enqueue(6, 1000000);
    tick();
    CHECK((reply[1] & 1) && reply[3] == 2);
    tick();
    tick();
    CHECK(reply[4] == 1);
    tick();
    CHECK(reply[4] == 2);
    service(5, 2);
    CHECK(reply[0]);
    tick();
    CHECK(reply[3] == 3 && (reply[1] & 1));
    tick();
    CHECK(reply[4] == 3 && !reply[16]);
    service(5, 3);
    CHECK(reply[0]);
    // Capacity pressure must not stop tick execution or existing regulation.
    for (unsigned i = 0; i < dmotor::ring_depth; ++i) {
        enqueue(1, 1000);
        tick();
        CHECK(!reply[32]);
    }
    enqueue(6, 1000000);
    tick();
    CHECK(reply[3] == (3 + dmotor::ring_depth) && reply[16] != 0 && !reply[32]);
    service(5, (3 + dmotor::ring_depth));
    tick();
    CHECK(reply[3] == (4 + dmotor::ring_depth));
    enqueue(1, 1000);
    // Immediate stop cancels active and pending work once, preserving autonomous stop.
    service(0, 0, 0, 0, false, false, true);
    CHECK(reply[4] == (4 + dmotor::ring_depth) && (reply[1] & 4));
    tick();
    tick();
    CHECK(reply[4] == (4 + dmotor::ring_depth) && !reply[16] && !reply[32]);
    service(7, (3 + dmotor::ring_depth), 2);
    CHECK(reply[7] == 2);
    authorized = false;
    tick();
    service(5, 4 + dmotor::ring_depth);
    service(6);
    service(3);
    CHECK(reply[0] && reply[5] == 2);
    authorized = true;
    enqueue(1, 1000, true);
    tick();
    CHECK(reply[4] == 1 && !reply[6]);
    service(7, 0, 2);
    CHECK(reply[7] == 1);
    service(7, 0, 4);
    CHECK(reply[7] == 0);
    // Feedback invalidation on this boundary must not capture on yesterday's readiness.
    enqueue(1, 1000);
    ++count;
    edge = now + 100000;
    tick();
    CHECK(reply[3] == 1 && !reply[6] && !reply[32]);
    now += 2800000; // service traffic does not substitute for 1 kHz hardware ticks: test fault next
    tick();
    CHECK(reply[32] && !reply[16] && (reply[1] & 4));
    reset();
    enqueue(1, 1000);
    tick(true);
    CHECK(reply[3] == 1 && reply[4] == 0 && !reply[6] && !reply[16]);
    // A between-tick capture fault stays latched even after its input pulse clears.
    reset();
    service(0, 0, 0, 0, false, false, false, true);
    CHECK(reply[32] & 2);
    tick();
    CHECK((reply[32] & 2) && !reply[16]);
    // A controller-side range fault on the offered command must not advertise
    // acceptance when protective cancellation prevented ring capture.
    cfg[1] = 1000;
    reset();
    enqueue(1, 1000);
    ++count;
    edge = now + 100000;
    tick();
    ++count;
    edge = now + 100000;
    tick();
    CHECK(reply[32] && !reply[6] && !(reply[18] & 8) && reply[4] == 0);
    cfg[1] = 130000;
    // Every descriptor error must be rejected by the shared validation path,
    // with correlation taken from ring ownership, never malformed wire fields.
    for (unsigned kind = 1; kind <= 8; ++kind) {
        reset();
        if (kind == 8) {
            // Reopen under generation 1, then supply generation 2 before the
            // controller's first tick: the ring's generation still governs ownership.
            authorized = false;
            service(0, 0, 0, 0, false, false, false, false, true);
            service(3);
            cfg[0] = 2;
            authorized = true;
        }
        dmotor::Descriptor d{};
        d[0] = 1;
        d[3] = d[4] = 1;
        d[5] = 1000;
        if (kind == 1) {
            d[0] = 0;
        }
        if (kind == 2) {
            d[12] = 1;
        }
        if (kind == 3) {
            d[2] = 99;
        }
        if (kind == 4) {
            d[3] = 99;
        }
        if (kind == 5) {
            d[4] = 99;
        }
        if (kind == 6) {
            d[5] = 130001;
        }
        if (kind == 7) {
            d[0] = 3;
            d[6] = 1;
            d[11] = 1000;
        }
        if (kind == 8) {
            d[4] = 2;
        }
        for (unsigned word = 0; word < d.size(); ++word) {
            service(1, 0, word, d[word]);
            CHECK(reply[0]);
        }
        service(2, 1, 1);
        CHECK(reply[0]);
        if (kind == 7) {
            position = INT64_MAX; // Changed after publication.
        }
        for (unsigned i = 0; i < 101; ++i) {
            tick();
        }
        CHECK(reply[3] == 1 && reply[4] == 1 && !reply[6] && !reply[32] && !reply[16]);
        service(7, 0, 0);
        CHECK(reply[7] == 0);
        service(7, 0, 1);
        CHECK(reply[7] == 1);
        service(7, 0, 2);
        CHECK(reply[7] == 1);
        service(7, 0, 3);
        CHECK(reply[7] == (kind == 8 ? 5 : kind));
        service(7, 0, 4);
        CHECK(reply[7] == 0);
        cfg[0] = 1;
    }
    std::cout << "dispatcher: exact activation, ordered completion, backpressure, stop, rejection "
                 "and cancellation priority passed\n";
}
