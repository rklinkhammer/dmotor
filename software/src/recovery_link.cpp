#include "dmotor_sw/recovery_link.hpp"
namespace dmotor_sw {
namespace recovery {
using monitor::Delivery;
using monitor::Frame;
using monitor::Type;
void Link::invalidate() {
    link_.invalidate();
    model_.reset();
    logical_sequence_ = 0;
    retry_ = {};
}
Frame Link::discover(uint64_t lifetime, bool inhibited) {
    invalidate();
    return link_.begin(lifetime, inhibited);
}
monitor::Reception Link::receive(const Frame& f, uint64_t now) {
    auto result = link_.receive(f);
    if (result.delivery == Delivery::re_pair_required || result.delivery == Delivery::exhausted) {
        invalidate();
        return result;
    }
    if (result.delivery == Delivery::paired) {
        model_.reset(new Model({link_.local_session(), link_.peer_session()}));
        logical_sequence_ = 0;
        retry_ = {};
    }
    if (result.delivery != Delivery::accepted || !model_)
        return result;
    if (f.type == Type::fault_evt || f.type == Type::nak) {
        // Conservative failure policy: neither event can authorize recovery.
        model_->abort();
        return result;
    }
    if (f.type != Type::recover_state && f.type != Type::commit_ack && f.type != Type::state_reply)
        return result; // Wrong-direction messages never reach the recovery model.
    const auto offset = f.type == Type::state_reply ? 8U : 0U;
    const auto* p = f.payload.data() + offset;
    if (p[0] > static_cast<uint8_t>(monitor::State::fault)) {
        model_->abort();
        return result;
    }
    Reply r;
    r.pair = {f.peer, f.source};
    r.transaction = f.transaction;
    r.epoch = f.epoch;
    r.challenge = offset ? monitor::get_le(f.payload.data(), 8) : 0;
    r.state = static_cast<monitor::State>(p[0]);
    r.causes = static_cast<uint32_t>(monitor::get_le(p + 1, 4));
    r.completed_transaction = static_cast<uint32_t>(monitor::get_le(p + 5, 4));
    r.completed_epoch = static_cast<uint32_t>(monitor::get_le(p + 9, 4));
    r.pl_session = static_cast<uint32_t>(monitor::get_le(p + 13, 4));
    if (f.type == Type::recover_state)
        model_->recovered(r, now);
    else if (f.type == Type::commit_ack)
        model_->committed(r, now);
    else
        model_->state_reply(r, now);
    return result; // Transport acceptance is not semantic success or admission.
}
bool Link::begin(const Cleanup& proof, uint64_t now) {
    return model_ && model_->begin(proof, now);
}
Frame Link::poll(uint64_t now) {
    if (!model_)
        return {};
    const auto tx = model_->poll(now);
    if (tx.message == Message::none)
        return {};
    if (logical_sequence_ == tx.sequence)
        return retry_; // Keep the exact wire identity on every logical retry.
    Frame f;
    f.epoch = tx.epoch;
    f.transaction = tx.transaction;
    switch (tx.message) {
        case Message::recover_request:
            f.type = Type::recover_req;
            f.length = 8;
            monitor::put_le(f.payload.data(), tx.cleanup.session, 4);
            monitor::put_le(f.payload.data() + 4,
                            (tx.cleanup.disabled ? 1U : 0U) | (tx.cleanup.drained ? 2U : 0U) |
                                (tx.cleanup.causes_clear ? 4U : 0U),
                            4);
            break;
        case Message::recover_commit:
            f.type = Type::recover_commit;
            break;
        case Message::state_query:
            f.type = Type::state_query;
            f.length = 8;
            monitor::put_le(f.payload.data(), tx.challenge, 8);
            break;
        default:
            return {};
    }
    if (!link_.send(f)) {
        invalidate();
        return {};
    }
    logical_sequence_ = tx.sequence;
    retry_ = f;
    return f;
}
bool Link::query_arm(uint64_t now) {
    return model_ && model_->query_arm(now);
}
bool Link::confirm_local(const Cleanup& proof, uint64_t now) {
    return model_ && model_->confirm_local(proof, now);
}
bool Link::admission_open() const {
    return link_.paired() && model_ && model_->admission_open();
}
bool Link::halt_required() const {
    return !model_ || model_->halt_required();
}
Phase Link::phase() const {
    return model_ ? model_->phase() : Phase::held;
}
} // namespace recovery
} // namespace dmotor_sw
