#pragma once
#include "dmotor/batch_registers.hpp"
namespace dmotor {
struct VelocityBatch {
    uint32_t session = 0, sequence = 0, mask = 0;
    uint32_t generation[batch_reg::CHANNELS] = {};
    int32_t velocity_mrpm[batch_reg::CHANNELS] = {};
    uint64_t tick = 0;
};
// Trusted scheduler/configuration inputs; none are ordinary software staging.
struct BatchAdmission {
    uint32_t session = 0;
    uint32_t authorized_mask = 0, ready_mask = 0;
    uint32_t generation[batch_reg::CHANNELS] = {};
    uint32_t max_speed_mrpm[batch_reg::CHANNELS] = {};
};
enum class BatchReject : uint32_t {
    none = batch_reg::REJECT_NONE,
    access = batch_reg::REJECT_ACCESS,
    incomplete = batch_reg::REJECT_INCOMPLETE,
    mask = batch_reg::REJECT_MASK,
    ownership = batch_reg::REJECT_OWNERSHIP,
    sequence = batch_reg::REJECT_SEQUENCE,
    speed = batch_reg::REJECT_SPEED,
    unavailable = batch_reg::REJECT_UNAVAILABLE,
    pending_mask = batch_reg::REJECT_PENDING_MASK,
    busy = batch_reg::REJECT_BUSY,
    retirement = batch_reg::REJECT_RETIREMENT,
};
struct BatchRead { uint32_t value; bool okay; };
class AtomicMailbox {
  public:
    // Integration must inhibit/drain the bus and arithmetic before session reset.
    void reset(uint32_t session);
    bool write(uint32_t address, uint32_t value, uint8_t strobes,
               const BatchAdmission& live);
    BatchRead read(uint32_t address) const;
    // Tick reservation is all-or-none. Not-ready retains pending unchanged.
    bool begin_tick(uint64_t tick, const BatchAdmission& live, VelocityBatch& batch);
    // Results are not published until all selected computations succeed.
    bool retire(uint32_t session, uint32_t sequence, const BatchAdmission& live,
                bool successful);
    // STOP/disable/fault invalidates entire intersecting batches, including in flight.
    void invalidate(uint32_t motor_mask);
    BatchReject rejection() const { return rejection_; }
  private:
    BatchReject validate(const VelocityBatch& batch, const BatchAdmission& live) const;
    bool reject(BatchReject reason);
    uint32_t session_ = 0, published_ = 0;
    VelocityBatch staged_{}, pending_{}, inflight_{}, applied_{};
    uint32_t motor_sequence_[batch_reg::CHANNELS] = {};
    uint64_t motor_tick_[batch_reg::CHANNELS] = {};
    uint32_t written_ = 0;
    bool pending_valid_ = false, inflight_valid_ = false;
    BatchReject rejection_ = BatchReject::none;
};
} // namespace dmotor
