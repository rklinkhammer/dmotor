#include "dmotor/protocol.hpp"
#include <cstdlib>
#include <iostream>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << __LINE__ << ": " #x "\n";                                                 \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
using namespace dmotor;
static void write(CommandRing& ring, uint32_t sequence, Descriptor d) {
    d[2] = sequence;
    d[3] = ring.epoch();
    d[4] = 7;
    for (uint32_t i = 0; i < 16; ++i) {
        CHECK(ring.stage(sequence & dmotor::ring_mask, i, d[i]));
    }
}
int main() {
    CHECK(signed_pair(0, 0x80000000) == INT64_MIN);
    CHECK(signed_pair(UINT32_MAX, UINT32_MAX) == -1);
    Descriptor velocity{};
    velocity[0] = 1;
    velocity[5] = 1000;
    CommandRing ring;
    CHECK(!ring.reopen(false, 7));
    CHECK(ring.reopen(true, 7, UINT32_MAX - dmotor::ring_depth / 2));
    const uint32_t base = ring.producer();
    for (uint32_t i = 0; i < dmotor::ring_depth; ++i) {
        write(ring, base + i, velocity);
    }
    CHECK(!ring.publish(base + dmotor::ring_depth + 1, ring.epoch()));
    CHECK(!ring.publish(base + dmotor::ring_depth, ring.epoch() + 1));
    CHECK(ring.publish(base + dmotor::ring_depth, ring.epoch()));
    CHECK(!ring.stage(base & dmotor::ring_mask, 5, 1));
    CHECK(ring.capture(1, 0, 130000));
    CHECK(ring.busy());
    CHECK(ring.stage(base & dmotor::ring_mask, 5, 999));
    CHECK(ring.active()[5] == 1000);
    CHECK(ring.finish(2));
    for (unsigned i = 1; i < dmotor::ring_depth; ++i) {
        CHECK(ring.capture(i * 2, 0, 130000));
        CHECK(ring.finish(i * 2 + 1));
    }
    write(ring, ring.producer(), velocity);
    CHECK(ring.publish(ring.producer() + 1, ring.epoch()));
    CHECK(!ring.capture(50, 0, 130000)); // Completion reservation backpressure.
    ring.cancel(51, 8);
    const Cancellation saved = ring.cancellation();
    ring.cancel(52, 16);
    CHECK(ring.cancellation().timestamp == saved.timestamp);
    CHECK(ring.cancellation().pending_end - saved.pending_begin == 1);
    CHECK(!ring.reopen(true, 7));
    CHECK(!ring.acknowledge_completions(ring.completion_producer() + 1));
    CHECK(ring.acknowledge_completions(ring.completion_producer()));
    ring.acknowledge_cancellation();
    CHECK(ring.reopen(true, 7));
    CHECK(!ring.publish(1, saved.epoch));
    Descriptor bad = velocity;
    bad[12] = 1;
    write(ring, 0, bad);
    CHECK(ring.publish(1, ring.epoch()));
    CHECK(ring.capture(100, 0, 130000));
    CHECK(!ring.busy());
    Completion completion{};
    CHECK(ring.read_completion(0, completion));
    CHECK(completion[2] == 1 && completion[3] == static_cast<uint32_t>(Error::reserved));
    CHECK(completion[4] == 0);
    Descriptor relative{};
    relative[0] = 3;
    relative[5] = 1000;
    relative[6] = 1;
    relative[11] = 100;
    CHECK(validate(relative, 0, 0, 0, INT64_MAX, 130000) == Error::overflow);
    relative[6] = UINT32_MAX;
    relative[7] = UINT32_MAX;
    CHECK(validate(relative, 0, 0, 0, INT64_MIN, 130000) == Error::overflow);
    for (uint32_t opcode = 1; opcode <= 6; ++opcode) {
        Descriptor d{};
        d[0] = opcode;
        if (opcode == 2 || opcode == 3) {
            d[5] = 1000;
            d[11] = 100;
        }
        if (opcode == 5) {
            d[9] = 100;
        }
        if (opcode == 6) {
            d[11] = 100;
        }
        CHECK(validate(d, 0, 0, 0, 0, 130000) == Error::none);
    }
    std::cout << "protocol: wrap, ownership, rejection, cancellation, backpressure passed\n";
}
