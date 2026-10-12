#pragma once
#include "dmotor/mailbox.hpp"
namespace dmotor {
constexpr unsigned mailbox_motor_count = 4;
}
// One transaction in flight. Reset addresses only the selected context.
extern "C" void dmotor_mailbox_shared(uint32_t motor, bool reset, const uint32_t request[4], bool tick,
    bool authorized, bool stop, bool capture_fault, uint32_t session, uint64_t now,
    uint64_t count, uint64_t last_edge, uint32_t direction_epoch,
    uint32_t invalid_transitions, bool negative, const uint32_t configuration[20],
    uint32_t reply[16]);
