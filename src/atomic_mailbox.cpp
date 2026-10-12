#include "dmotor/atomic_mailbox.hpp"
namespace dmotor {
namespace {
constexpr uint32_t motor_mask = (1U << batch_reg::CHANNELS) - 1U;
constexpr uint32_t header_written = 7U;
int32_t signed_value(uint32_t v) {
    return v <= 0x7fffffffU ? static_cast<int32_t>(v)
        : static_cast<int32_t>(static_cast<int64_t>(v) - 0x100000000LL);
}
}
void AtomicMailbox::reset(uint32_t session) {
    *this = AtomicMailbox{};
    session_ = session;
}
bool AtomicMailbox::reject(BatchReject reason) {
    rejection_ = reason;
    return false;
}
BatchReject AtomicMailbox::validate(const VelocityBatch& b, const BatchAdmission& live) const {
    if (!b.mask || (b.mask & ~motor_mask)) return BatchReject::mask;
    if (!session_ || b.session != session_ || live.session != session_)
        return BatchReject::ownership;
    if ((b.mask & live.authorized_mask) != b.mask) return BatchReject::unavailable;
    for (unsigned m = 0; m < batch_reg::CHANNELS; ++m) {
        if (!(b.mask & (1U << m))) continue;
        if (!b.generation[m] || b.generation[m] != live.generation[m])
            return BatchReject::ownership;
        const int64_t speed = b.velocity_mrpm[m];
        const uint64_t magnitude = static_cast<uint64_t>(speed < 0 ? -speed : speed);
        if (!live.max_speed_mrpm[m] || magnitude > live.max_speed_mrpm[m])
            return BatchReject::speed;
    }
    return BatchReject::none;
}
bool AtomicMailbox::write(uint32_t address, uint32_t value, uint8_t strobes,
                          const BatchAdmission& live) {
    using namespace batch_reg;
    if ((address & 3U) || strobes != 15U) return reject(BatchReject::access);
    if (address == STAGE_SESSION) { staged_.session = value; written_ |= 1U; return true; }
    if (address == STAGE_SEQUENCE) { staged_.sequence = value; written_ |= 2U; return true; }
    if (address == STAGE_MASK) { staged_.mask = value; written_ |= 4U; return true; }
    if (address >= STAGE_MOTOR_BASE && address < STAGE_MOTOR_BASE + CHANNELS*STAGE_MOTOR_STRIDE) {
        const unsigned m = (address - STAGE_MOTOR_BASE) / STAGE_MOTOR_STRIDE;
        const unsigned field = (address - STAGE_MOTOR_BASE) % STAGE_MOTOR_STRIDE;
        if (field == GENERATION) staged_.generation[m] = value;
        else staged_.velocity_mrpm[m] = signed_value(value);
        written_ |= 1U << (3U + 2U*m + (field == GENERATION ? 0U : 1U));
        return true;
    }
    if (address != COMMIT) return reject(BatchReject::access);
    const uint32_t supplied = written_;
    written_ = 0; // Every commit attempt requires complete restaging next time.
    if (value != 1U) return reject(BatchReject::access);
    uint32_t required = header_written;
    for (unsigned m = 0; m < CHANNELS; ++m)
        if (staged_.mask & (1U << m)) required |= 3U << (3U + 2U*m);
    if ((supplied & required) != required) return reject(BatchReject::incomplete);
    const auto reason = validate(staged_, live);
    if (reason != BatchReject::none) return reject(reason);
    if (!staged_.sequence || staged_.sequence <= published_) return reject(BatchReject::sequence);
    if (pending_valid_ && staged_.mask != pending_.mask) return reject(BatchReject::pending_mask);
    pending_ = staged_;
    pending_valid_ = true;
    published_ = pending_.sequence;
    return true;
}
BatchRead AtomicMailbox::read(uint32_t address) const {
    using namespace batch_reg;
    if (address & 3U) return {0, false};
    switch (address) {
    case IDENTITY: return {PROFILE_ID, true};
    case SCHEMA: return {SCHEMA_VERSION, true};
    case MOTOR_COUNT: return {CHANNELS, true};
    case SESSION: return {session_, true};
    case STAGE_SESSION: return {staged_.session, true};
    case STAGE_SEQUENCE: return {staged_.sequence, true};
    case STAGE_MASK: return {staged_.mask, true};
    case COMMIT: return {0, true};
    case PUBLISHED_SEQUENCE: return {published_, true};
    case PENDING_SEQUENCE: return {pending_valid_ ? pending_.sequence : 0, true};
    case PENDING_MASK: return {pending_valid_ ? pending_.mask : 0, true};
    case INFLIGHT_SEQUENCE: return {inflight_valid_ ? inflight_.sequence : 0, true};
    case APPLIED_SEQUENCE: return {applied_.sequence, true};
    case APPLIED_MASK: return {applied_.mask, true};
    case APPLIED_TICK_LO: return {static_cast<uint32_t>(applied_.tick), true};
    case APPLIED_TICK_HI: return {static_cast<uint32_t>(applied_.tick >> 32), true};
    case REJECT_REASON: return {static_cast<uint32_t>(rejection_), true};
    default: break;
    }
    if (address >= STAGE_MOTOR_BASE && address < STAGE_MOTOR_BASE + CHANNELS*STAGE_MOTOR_STRIDE) {
        const unsigned m = (address - STAGE_MOTOR_BASE) / STAGE_MOTOR_STRIDE;
        const unsigned field = (address - STAGE_MOTOR_BASE) % STAGE_MOTOR_STRIDE;
        return {field == GENERATION ? staged_.generation[m]
            : static_cast<uint32_t>(staged_.velocity_mrpm[m]), true};
    }
    if (address >= APPLIED_MOTOR_BASE && address < APPLIED_MOTOR_BASE + CHANNELS*APPLIED_MOTOR_STRIDE) {
        const unsigned m = (address - APPLIED_MOTOR_BASE) / APPLIED_MOTOR_STRIDE;
        switch ((address - APPLIED_MOTOR_BASE) % APPLIED_MOTOR_STRIDE) {
        case 0: return {motor_sequence_[m], true};
        case 4: return {static_cast<uint32_t>(motor_tick_[m]), true};
        case 8: return {static_cast<uint32_t>(motor_tick_[m] >> 32), true};
        default: return {0, false};
        }
    }
    return {0, false};
}
bool AtomicMailbox::begin_tick(uint64_t tick, const BatchAdmission& live, VelocityBatch& batch) {
    if (inflight_valid_) return reject(BatchReject::busy);
    if (!pending_valid_) return false;
    const auto reason = validate(pending_, live);
    if (reason != BatchReject::none) { pending_valid_ = false; return reject(reason); }
    if ((pending_.mask & live.ready_mask) != pending_.mask) return false;
    inflight_ = pending_;
    inflight_.tick = tick;
    inflight_valid_ = true;
    pending_valid_ = false;
    batch = inflight_;
    return true;
}
bool AtomicMailbox::retire(uint32_t session, uint32_t sequence, const BatchAdmission& live,
                           bool successful) {
    if (!inflight_valid_ || session != inflight_.session || sequence != inflight_.sequence)
        return reject(BatchReject::retirement);
    inflight_valid_ = false;
    const auto reason = validate(inflight_, live);
    if (!successful || reason != BatchReject::none) {
        // Failed frame invalidates any newer intersecting intent, too.
        invalidate(inflight_.mask);
        return reject(reason != BatchReject::none ? reason : BatchReject::retirement);
    }
    applied_ = inflight_;
    for (unsigned m = 0; m < batch_reg::CHANNELS; ++m) {
        if (applied_.mask & (1U << m)) {
            motor_sequence_[m] = applied_.sequence;
            motor_tick_[m] = applied_.tick;
        }
    }
    return true;
}
void AtomicMailbox::invalidate(uint32_t mask) {
    if (pending_valid_ && (pending_.mask & mask)) pending_valid_ = false;
    if (inflight_valid_ && (inflight_.mask & mask)) inflight_valid_ = false;
    if (mask) written_ = 0; // Staging mask may not have been written yet.
}
} // namespace dmotor
