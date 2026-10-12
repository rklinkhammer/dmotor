#pragma once
#include "dmotor/atomic_mailbox.hpp"
namespace dmotor {
// Io provides bool write(offset,value), uint32_t read(offset), and fence().
// The driver must serialize all writers; fence must implement the platform's
// device-write completion/ordering contract, not merely a compiler barrier.
template<class Io> bool publish_velocity_batch(Io& io, const VelocityBatch& b) {
    using namespace batch_reg;
    if (io.read(IDENTITY) != PROFILE_ID || io.read(SCHEMA) != SCHEMA_VERSION ||
        io.read(MOTOR_COUNT) != CHANNELS || io.read(SESSION) != b.session ||
        !b.session || !b.sequence || !b.mask || b.mask >= (1U << CHANNELS)) return false;
    if (!io.write(STAGE_SESSION,b.session) || !io.write(STAGE_SEQUENCE,b.sequence) ||
        !io.write(STAGE_MASK,b.mask)) return false;
    for (unsigned m=0; m<CHANNELS; ++m) {
        if (!(b.mask & (1U << m))) continue;
        const uint32_t base = STAGE_MOTOR_BASE + m*STAGE_MOTOR_STRIDE;
        if (!io.write(base+GENERATION,b.generation[m]) ||
            !io.write(base+VELOCITY_MRPM,static_cast<uint32_t>(b.velocity_mrpm[m]))) return false;
    }
    io.fence();
    if (!io.write(COMMIT,1)) return false;
    io.fence();
    return io.read(SESSION)==b.session && io.read(PUBLISHED_SEQUENCE)==b.sequence;
}
struct AppliedBatchSnapshot {
    uint32_t session=0, sequence=0, mask=0;
    uint64_t tick=0;
};
// One bounded attempt; caller may retry. A successful publication is not a
// physical speed/position completion. Sequence reuse requires drained reset.
template<class Io> bool read_applied_batch(Io& io, AppliedBatchSnapshot& result) {
    using namespace batch_reg;
    AppliedBatchSnapshot sample;
    sample.session=io.read(SESSION);
    sample.sequence=io.read(APPLIED_SEQUENCE);
    sample.mask=io.read(APPLIED_MASK);
    const uint32_t low=io.read(APPLIED_TICK_LO), high=io.read(APPLIED_TICK_HI);
    sample.tick=(uint64_t{high} << 32) | low;
    io.fence();
    if (io.read(APPLIED_SEQUENCE)!=sample.sequence || io.read(SESSION)!=sample.session) return false;
    result=sample;
    return true;
}
} // namespace dmotor
