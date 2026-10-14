#include "dmotor_sw/model.hpp"
#include <limits>
#include <stdexcept>

namespace dmotor_sw {
namespace {
struct MailboxIo {
    dmotor::AtomicMailbox& mailbox;
    const dmotor::BatchAdmission& admission;
    bool write(uint32_t address, uint32_t value) {
        return mailbox.write(address, value, 15, admission);
    }
    uint32_t read(uint32_t address) const { return mailbox.read(address).value; }
    void fence() const {} // In-process sequential accesses only; NOT an MMIO fence.
};
bool valid_mask(uint32_t mask) { return mask && !(mask & ~all_motors); }
}
Model::Model(Timeouts timeouts) : timeouts_(timeouts) {
    if (!timeouts.authorization_ticks || !timeouts.target_ticks || !timeouts.maintenance_lease_ticks)
        throw std::invalid_argument("Timeouts must be nonzero control ticks");
}
bool Model::owns(uint64_t owner) const { return owner && owner == owner_; }
uint64_t Model::acquire() {
    if (owner_ || next_owner_ == std::numeric_limits<uint64_t>::max()) return 0;
    owner_ = ++next_owner_;
    lease_tick_ = state_.tick;
    return owner_;
}
bool Model::renew(uint64_t owner) {
    if (!owns(owner)) return false;
    lease_tick_ = state_.tick;
    return true;
}
void Model::trip(Fault fault) {
    if (state_.fault == Fault::none) state_.fault = fault;
    state_.halted = true;
    state_.enabled_mask = state_.tracking_mask = refresh_mask_ = 0;
    state_.requested_mrpm.fill(0);
    admission_.authorized_mask = admission_.ready_mask = 0;
    mailbox_.invalidate(all_motors);
}
bool Model::release(uint64_t owner) {
    if (!owns(owner)) return false;
    trip(Fault::ownership_lost);
    owner_ = 0;
    return true;
}
bool Model::rearm(uint64_t owner, uint32_t session,
                  const std::array<MotorLimits, motor_count>& limits) {
    if (!owns(owner) || state_.enabled_mask || !session || session <= last_session_) return false;
    for (const auto& limit : limits)
        if (!limit.generation || !limit.maximum_mrpm || limit.maximum_mrpm > maximum_velocity_mrpm)
            return false;
    admission_ = {};
    admission_.session = last_session_ = session;
    for (unsigned m = 0; m < motor_count; ++m) {
        admission_.generation[m] = limits[m].generation;
        admission_.max_speed_mrpm[m] = limits[m].maximum_mrpm;
    }
    mailbox_.reset(session);
    const auto tick = state_.tick;
    state_ = {};
    state_.tick = tick;
    state_.halted = false;
    refresh_mask_ = 0;
    return true;
}
bool Model::enable(uint64_t owner, uint32_t mask) {
    if (!owns(owner) || state_.halted || !valid_mask(mask)) return false;
    for (unsigned m = 0; m < motor_count; ++m)
        if ((mask & ~state_.enabled_mask) & (1U << m)) authorization_tick_[m] = state_.tick;
    state_.enabled_mask |= mask;
    admission_.authorized_mask = admission_.ready_mask = state_.enabled_mask;
    return true; // Warm-up/numeric configuration validation is outside this model.
}
bool Model::publish(uint64_t owner, const dmotor::VelocityBatch& batch) {
    if (!owns(owner) || state_.halted) return false;
    MailboxIo io{mailbox_, admission_};
    return dmotor::publish_velocity_batch(io, batch);
}
bool Model::refresh(uint64_t owner, uint32_t mask) {
    if (!owns(owner) || state_.halted || !valid_mask(mask) || (mask & ~state_.enabled_mask)) return false;
    refresh_mask_ |= mask;
    return true;
}
bool Model::halt(uint64_t owner) {
    if (!owns(owner)) return false;
    trip(Fault::requested_halt);
    return true;
}
void Model::advance(uint32_t ticks) {
    while (ticks--) {
        if (state_.tick == std::numeric_limits<uint64_t>::max())
            throw std::overflow_error("Model clock exhausted");
        ++state_.tick;
        if (owner_ && state_.tick - lease_tick_ >= timeouts_.maintenance_lease_ticks) {
            trip(Fault::ownership_lost);
            owner_ = 0;
        }
        if (state_.halted) continue;
        // All expiration checks precede refresh and batch activation.
        for (unsigned m = 0; m < motor_count; ++m) {
            const uint32_t bit = 1U << m;
            if ((state_.enabled_mask & bit) &&
                state_.tick - authorization_tick_[m] >= timeouts_.authorization_ticks)
                trip(Fault::authorization);
            if ((state_.tracking_mask & bit) &&
                state_.tick - target_tick_[m] >= timeouts_.target_ticks)
                trip(Fault::target_age);
        }
        if (state_.halted) continue;
        for (unsigned m = 0; m < motor_count; ++m)
            if (refresh_mask_ & (1U << m)) authorization_tick_[m] = state_.tick;
        refresh_mask_ = 0;
        dmotor::VelocityBatch batch;
        if (mailbox_.begin_tick(state_.tick, admission_, batch) &&
            mailbox_.retire(batch.session, batch.sequence, admission_, true)) {
            for (unsigned m = 0; m < motor_count; ++m) {
                if (!(batch.mask & (1U << m))) continue;
                state_.requested_mrpm[m] = batch.velocity_mrpm[m];
                target_tick_[m] = state_.tick;
            }
            state_.tracking_mask |= batch.mask;
            state_.applied = {batch.session, batch.sequence, batch.mask, batch.tick};
        }
    }
}
Snapshot Model::snapshot() const { return state_; }
} // namespace dmotor_sw
