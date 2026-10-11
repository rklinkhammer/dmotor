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

// Integration reference: serialized ring capture/retirement at control boundaries.
// The future hardware dispatcher must additionally correlate epoch and sequence.
int main() {
    dmotor::CommandRing ring;
    dmotor::MotionExecutor motor;
    dmotor::MotionConfig c{1,   130000, 1000000, 1000000, 10296, 5,     1,
                           100, 2,      655,     65536,   65536, 65536, 1000000};
    CHECK(ring.reopen(true, 1));
    uint64_t now = 0;
    auto enqueue = [&](uint32_t opcode, uint32_t value) {
        const auto sequence = ring.producer();
        dmotor::Descriptor d{};
        d[0] = opcode;
        d[2] = sequence;
        d[3] = ring.epoch();
        d[4] = 1;
        if (opcode == 1) {
            d[5] = value;
        } else if (opcode == 5) {
            d[9] = value;
        } else if (opcode == 6) {
            d[11] = value;
        }
        for (uint32_t i = 0; i < 16; ++i) {
            CHECK(ring.stage(sequence & dmotor::ring_mask, i, d[i]));
        }
        CHECK(ring.publish(sequence + 1, ring.epoch()));
    };
    dmotor::MotionResult r;
    bool ack = false;
    auto tick = [&]() {
        now += 100000;
        bool captured = false;
        if ((r.ready || ack) && !ring.busy()) {
            captured = ring.capture(now, 0, c.max_speed_mrpm);
        }
        r = motor.step(false, true, captured && ring.busy(), ring.active(), ack, false,
                       ring.epoch(), now, 0, 0, true, c);
        ack = false;
        CHECK(!r.faults);
        if (r.completion_valid) {
            CHECK(ring.busy());
            CHECK(r.sequence == ring.active()[2] && r.epoch == ring.epoch());
            CHECK(ring.finish(r.terminal, r.outcome, r.detail));
            ack = true;
        }
    };
    tick(); // establish ready without CPU control ticks
    enqueue(1, 10000);
    enqueue(5, 3000);
    tick();
    enqueue(6, 1000000);
    CHECK(r.completion_valid && r.ramp_mrpm == 1000);
    tick();
    CHECK(r.busy && ring.consumer() == 2);
    tick();
    tick();
    CHECK(r.busy);
    tick();
    CHECK(r.completion_valid && r.ramp_mrpm == 5000);
    CHECK(ring.acknowledge_completions(2));
    tick();
    CHECK(r.busy && r.ramp_mrpm == 4000);
    for (unsigned i = 0; i < 5; ++i) {
        tick();
    }
    CHECK(!r.busy && r.duty_q16 == 0 && ring.completion_producer() == 3);
    CHECK(ring.acknowledge_completions(3));
    // Fill all completion slots. Backpressure blocks capture, never the PI ticks.
    for (unsigned i = 0; i < dmotor::ring_depth; ++i) {
        enqueue(1, 10000);
        tick();
    }
    CHECK(ring.completion_producer() == (3 + dmotor::ring_depth));
    enqueue(6, 1000000);
    for (unsigned i = 0; i < 10; ++i) {
        tick();
    }
    CHECK(ring.consumer() == (3 + dmotor::ring_depth) && r.ramp_mrpm == 10000 && r.duty_q16 != 0);
    CHECK(ring.acknowledge_completions((3 + dmotor::ring_depth)));
    tick();
    CHECK(r.busy && ring.consumer() == (4 + dmotor::ring_depth));
    enqueue(1, 1000);
    // Live protection cancels the active and pending work; executor output is zero.
    now += 100000;
    ring.cancel(now, 8);
    r = motor.step(false, false, false, ring.active(), false, false, ring.epoch(), now, 0, 0, true,
                   c);
    CHECK(r.duty_q16 == 0 && !r.busy && !r.ready);
    CHECK(ring.cancellation().active_valid &&
          ring.cancellation().pending_begin == (4 + dmotor::ring_depth) &&
          ring.cancellation().pending_end == (5 + dmotor::ring_depth));
    CHECK(!ring.finish(now, r.outcome, r.detail)); // no duplicate retirement after cancellation
    dmotor::Completion completion{};
    CHECK(ring.read_completion((3 + dmotor::ring_depth), completion) && completion[2] == 2);
    std::cout
        << "motion queue: ordered execution, dwell, stop, backpressure and cancellation passed\n";
}
