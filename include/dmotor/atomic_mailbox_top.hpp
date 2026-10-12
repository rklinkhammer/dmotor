#pragma once
#include "dmotor/atomic_mailbox.hpp"
namespace dmotor {
// Internal scheduler test/handshake interface, not the software MMIO ABI.
enum class BatchOperation : uint32_t { reset, write, read, begin, retire, invalidate };
void batch_transaction(AtomicMailbox& mailbox, const uint32_t request[5],
                       uint64_t tick, const BatchAdmission& live, uint32_t reply[16]);
}
extern "C" void dmotor_atomic_mailbox(const uint32_t request[5], uint64_t tick,
                                      const uint32_t admission[11], uint32_t reply[16]);
