#include "dmotor/shared_mailbox.hpp"
#ifdef DMOTOR_USE_AP_INT
#include <ap_int.h>
#endif
extern "C" void dmotor_mailbox_shared(uint32_t motor, bool reset, const uint32_t request[4], bool tick,
    bool authorized, bool stop, bool capture_fault, uint32_t session, uint64_t now,
    uint64_t count, uint64_t last_edge, uint32_t direction_epoch,
    uint32_t invalid_transitions, bool negative, const uint32_t configuration[20],
    uint32_t reply[16]) {
    static dmotor::VelocityMailbox contexts[dmotor::mailbox_motor_count];
    if (motor >= dmotor::mailbox_motor_count) {
        for (unsigned i = 0; i < 16; ++i) reply[i] = 0;
        reply[9] = 16; // Invalid context; no state may be modified.
        return;
    }
    const dmotor::EncoderSample sample{count, last_edge, direction_epoch,
                                       invalid_transitions, negative};
    // Dynamic context selection at a single call site shares the computation.
    static_assert(dmotor::mailbox_motor_count == 4, "Update selector width with context count");
#ifdef DMOTOR_USE_AP_INT
    const ap_uint<2> selected = motor;
#else
    const unsigned selected = motor;
#endif
    contexts[selected].service(reset, request, tick, authorized, stop, capture_fault,
        session, now, sample, dmotor::decode_controller_config(configuration), reply);
}
