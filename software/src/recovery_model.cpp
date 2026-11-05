#include "dmotor_sw/recovery_model.hpp"
#include <limits>
namespace dmotor_sw {
namespace recovery {
namespace {
constexpr uint64_t timeout_us = 100000;
bool valid(const Cleanup& c) {
    return c.session && c.request && c.configuration && c.disabled && c.drained && c.causes_clear;
}
bool same(const Cleanup& a, const Cleanup& b) {
    return valid(a) && a.session == b.session && a.request == b.request &&
           a.configuration == b.configuration;
}
} // namespace
Model::Model(Pair pair) : pair_(pair) {}
void Model::abort() {
    phase_ = Phase::failed;
    halt_ = true;
    queued_ = false;
}
bool Model::clock(uint64_t now) {
    if (now < last_now_) {
        abort();
        return false;
    }
    last_now_ = now;
    return true;
}
bool Model::send(Message message, Phase phase, uint64_t now) {
    if (sequence_ == UINT32_MAX || now > UINT64_MAX - timeout_us) {
        abort();
        return false;
    }
    transmission_ = {message, pair_, transaction_, epoch_, ++sequence_, challenge_, cleanup_};
    phase_ = phase;
    queued_ = true;
    attempts_ = 0;
    sent_at_ = now;
    return true;
}
bool Model::begin(const Cleanup& cleanup, uint64_t now) {
    if (!clock(now))
        return false;
    if (phase_ != Phase::held && phase_ != Phase::failed && phase_ != Phase::unknown)
        return false;
    if (!pair_.ps || !pair_.monitor || !valid(cleanup) || cleanup.request <= last_request_ ||
        cleanup.session <= last_pl_session_ || transaction_ == UINT32_MAX) {
        abort();
        return false;
    }
    cleanup_ = cleanup;
    last_request_ = cleanup.request;
    last_pl_session_ = cleanup.session;
    ++transaction_;
    epoch_ = 0;
    // Caller has independently released PS HALT after cleanup; admission stays held.
    halt_ = false;
    return send(Message::recover_request, Phase::request, now);
}
bool Model::matches(const Reply& r) const {
    return r.pair.ps == pair_.ps && r.pair.monitor == pair_.monitor &&
           r.transaction == transaction_;
}
bool Model::completed(const Reply& r) const {
    return r.completed_transaction == transaction_ && r.completed_epoch == epoch_ &&
           r.pl_session == cleanup_.session && r.causes == 0;
}
bool Model::recovered(const Reply& r, uint64_t now) {
    if (!clock(now) || phase_ != Phase::request || !attempts_ || now - sent_at_ >= timeout_us ||
        !matches(r))
        return false;
    if (r.causes || (r.state != monitor::State::safe && r.state != monitor::State::fault)) {
        abort();
        return false;
    }
    epoch_ = r.epoch;
    return send(Message::recover_commit, Phase::commit, now);
}
bool Model::query(uint64_t now) {
    if (challenge_ == UINT64_MAX) {
        abort();
        return false;
    }
    ++challenge_;
    return send(Message::state_query, Phase::query, now);
}
bool Model::committed(const Reply& r, uint64_t now) {
    if (!clock(now) || phase_ != Phase::commit || !attempts_ || now - sent_at_ >= timeout_us ||
        !matches(r))
        return false;
    if (r.epoch != epoch_ || !completed(r) ||
        (r.state != monitor::State::safe && r.state != monitor::State::arm_pending &&
         r.state != monitor::State::armed)) {
        abort();
        return false;
    }
    return query(now); // Even an ARMED COMMIT_ACK is not a fresh challenge reply.
}
bool Model::state_reply(const Reply& r, uint64_t now) {
    if (!clock(now) || phase_ != Phase::query || !attempts_ || now - sent_at_ >= timeout_us ||
        !matches(r) || r.challenge != challenge_)
        return false;
    if (r.epoch != epoch_ || !completed(r)) {
        abort();
        return false;
    }
    queued_ = false;
    sent_at_ = now;
    if (r.state == monitor::State::armed)
        phase_ = Phase::local_check;
    else if (r.state == monitor::State::safe || r.state == monitor::State::arm_pending)
        phase_ = Phase::waiting_arm;
    else {
        abort();
        return false;
    }
    return true;
}
bool Model::query_arm(uint64_t now) {
    return clock(now) && phase_ == Phase::waiting_arm && query(now);
}
bool Model::confirm_local(const Cleanup& cleanup, uint64_t now) {
    if (!clock(now) || phase_ != Phase::local_check)
        return false;
    if (now - sent_at_ >= timeout_us || !same(cleanup, cleanup_)) {
        abort();
        return false;
    }
    phase_ = Phase::complete;
    return true;
}
Transmission Model::poll(uint64_t now) {
    if (!clock(now))
        return {};
    if (phase_ != Phase::request && phase_ != Phase::commit && phase_ != Phase::query)
        return {};
    if (!queued_ && now - sent_at_ < timeout_us)
        return {};
    if (attempts_ == 3) {
        if (phase_ == Phase::commit) {
            if (!query(now))
                return {};
        } else {
            const bool uncertain = phase_ == Phase::query;
            abort();
            if (uncertain)
                phase_ = Phase::unknown;
            return {};
        }
    }
    if (now > UINT64_MAX - timeout_us) {
        abort();
        return {};
    }
    queued_ = false;
    ++attempts_;
    sent_at_ = now;
    return transmission_; // Retries preserve every field of the logical request.
}
bool Model::allowed(Operation operation) const {
    switch (operation) {
        case Operation::ping:
        case Operation::status:
        case Operation::capture:
        case Operation::disable:
        case Operation::halt:
        case Operation::quit:
            return true;
        case Operation::prepare:
            return phase_ == Phase::held || phase_ == Phase::failed || phase_ == Phase::unknown ||
                   phase_ == Phase::complete;
        default:
            return admission_open(); // Necessary only: normal Runtime checks still apply.
    }
}
} // namespace recovery
} // namespace dmotor_sw
