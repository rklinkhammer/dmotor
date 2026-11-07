#include "dmotor_sw/recovery_coordinator.hpp"
#include <utility>
namespace dmotor_sw {
namespace recovery {
Coordinator::Coordinator(Runtime& runtime, std::function<uint64_t()> reserve_session)
    : runtime_(runtime), reserve_session_(std::move(reserve_session)) {
    runtime_.hold_for_recovery();
}
Coordinator::~Coordinator() {
    try {
        cancel();
    } catch (...) {
    }
}
void Coordinator::cancel() {
    owner_ = generation_ = lifetime_ = 0;
    hello_ = {};
    prepared_ = recovering_ = false;
    link_.invalidate(); // Revoke software authority before potentially failing hardware access.
    runtime_.hold_for_recovery();
}
void Coordinator::prepare(uint64_t owner, const std::array<MotorConfiguration, 4>& configs,
                          uint32_t mask) {
    if (!owner || recovering_)
        throw IoError("Recovery owner invalid or exchange in flight");
    cancel();
    try {
        // HALT has been confirmed by cancel(); block on storage only while inhibited.
        if (runtime_.status().enabled || !reserve_session_)
            throw IoError("Session allocation requires disabled outputs and a durable source");
        const auto reserved = reserve_session_();
        if (!reserved || reserved <= last_lifetime_)
            throw IoError("Endpoint session source reused or exhausted a lifetime");
        last_lifetime_ = lifetime_ = reserved; // Burn even if subsequent prepare fails.
        runtime_.prepare(configs, mask);
        owner_ = owner;
        generation_ = runtime_.recovery_generation();
        prepared_ = true;
    } catch (...) {
        try {
            cancel();
        } catch (...) {
        }
        throw;
    }
}
monitor::Frame Coordinator::discover() {
    if (!prepared_ || recovering_ || runtime_.recovery_generation() != generation_ ||
        !runtime_.recovery_held())
        return {};
    if (!lifetime_)
        return {};
    if (!monitor::valid(hello_))
        hello_ = link_.discover(lifetime_, true);
    return hello_; // Discovery retries retain identity; allocation happens only in prepare.
}
bool Coordinator::begin() {
    if (!prepared_ || recovering_ || runtime_.recovery_generation() != generation_ ||
        !runtime_.recovery_held())
        return false;
    try {
        const auto proof = runtime_.attest_cleanup(generation_);
        recovering_ = link_.begin(proof, runtime_.now_us());
        return recovering_;
    } catch (...) {
        try {
            cancel();
        } catch (...) {
        }
        throw;
    }
}
void Coordinator::check() {
    if (owner_ &&
        (runtime_.recovery_generation() != generation_ || (recovering_ && link_.halt_required())))
        cancel();
}
monitor::Reception Coordinator::receive(const monitor::Frame& frame, uint64_t now) {
    check();
    auto result = link_.receive(frame, now);
    if (result.delivery == monitor::Delivery::re_pair_required ||
        result.delivery == monitor::Delivery::exhausted)
        cancel();
    else
        check();
    return result;
}
monitor::Frame Coordinator::poll(uint64_t now) {
    check();
    auto frame = link_.poll(now);
    check();
    if (monitor::valid(frame)) {
        try {
            runtime_.attest_cleanup(generation_);
        } catch (...) {
            try {
                cancel();
            } catch (...) {
            }
            throw;
        }
    }
    return frame;
}
bool Coordinator::query_arm(uint64_t now) {
    check();
    const bool result = link_.query_arm(now);
    check();
    return result;
}
bool Coordinator::finish() {
    check();
    if (!owner_ || !recovering_ || link_.phase() != Phase::local_check)
        return false;
    try {
        const auto proof = runtime_.attest_cleanup(generation_);
        if (!link_.confirm_local(proof, runtime_.now_us())) {
            check();
            return false;
        }
        runtime_.complete_recovery(generation_);
        return true;
    } catch (...) {
        try {
            cancel();
        } catch (...) {
        }
        throw;
    }
}
bool Coordinator::admitted() const {
    return owner_ && generation_ == runtime_.recovery_generation() && !runtime_.recovery_held() &&
           link_.admission_open();
}
RecoveryStatus Coordinator::status() const {
    if (admitted()) {
        return {RecoveryState::admitted, true};
    }
    if (!owner_ || generation_ != runtime_.recovery_generation()) {
        return {RecoveryState::held, false};
    }
    if (!recovering_) {
        return {RecoveryState::pairing, false};
    }
    switch (link_.phase()) {
        case Phase::waiting_arm:
            return {RecoveryState::waiting_arm, false};
        case Phase::local_check:
            return {RecoveryState::verifying, false};
        case Phase::failed:
            return {RecoveryState::failed, false};
        case Phase::unknown:
            return {RecoveryState::unknown, false};
        case Phase::request:
        case Phase::commit:
        case Phase::query:
            return {RecoveryState::recovering, false};
        default:
            return {RecoveryState::held, false};
    }
}
Coordinator::Ticket Coordinator::ticket() const {
    return admitted() ? Ticket{owner_, generation_} : Ticket{};
}
bool Coordinator::current(Ticket t) const {
    return admitted() && t.owner == owner_ && t.generation == generation_;
}
bool Coordinator::enable(Ticket t, uint32_t mask) {
    if (!current(t))
        return false;
    try {
        runtime_.enable(mask);
        return true;
    } catch (...) {
        try {
            cancel();
        } catch (...) {
        }
        throw;
    }
}
bool Coordinator::velocity(Ticket t, uint32_t mask, const std::array<int32_t, 4>& targets) {
    if (!current(t))
        return false;
    try {
        runtime_.velocity(mask, targets);
        return true;
    } catch (...) {
        try {
            cancel();
        } catch (...) {
        }
        throw;
    }
}
bool Coordinator::stop(Ticket t, uint32_t mask) {
    if (!current(t))
        return false;
    try {
        runtime_.stop(mask);
        return true;
    } catch (...) {
        try {
            cancel();
        } catch (...) {
        }
        throw;
    }
}
void Coordinator::tick() {
    check();
    try {
        runtime_.tick();
        check();
    } catch (...) {
        try {
            cancel();
        } catch (...) {
        }
        throw;
    }
}
} // namespace recovery
} // namespace dmotor_sw
