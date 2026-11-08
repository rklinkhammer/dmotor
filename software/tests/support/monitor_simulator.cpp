#include "monitor_simulator.hpp"
namespace dmotor_sw {
namespace monitor {
namespace {
constexpr uint64_t period = 16000, debounce = 20000, staged_lifetime = 300000;
constexpr uint32_t scheduling_fault = 0x80000000U;
} // namespace
void Simulator::clear_exchange() {
    pending_ = completed_ = false;
    transaction_ = pl_session_ = 0;
    button_ = Button::released_needed;
    healthy_checks_ = 0;
}
Frame Simulator::boot(uint64_t lifetime) {
    clear_exchange();
    state_ = State::boot;
    sampled_ = was_pressed_ = false;
    now_ = sample_at_ = button_at_ = pending_at_ = 0;
    active_ = latched_ = epoch_ = last_transaction_ = 0;
    commits_ = 0;
    auto hello = link_.begin(lifetime, true);
    locked_ = !valid(hello);
    return hello;
}
void Simulator::fault(uint32_t causes) {
    clear_exchange();
    state_ = State::fault;
    latched_ |= causes;
    if (epoch_ == UINT32_MAX) {
        locked_ = true;
        link_.invalidate();
    } else
        ++epoch_;
}
bool Simulator::clock(uint64_t now) {
    if (locked_)
        return false;
    if (now < now_) {
        fault(scheduling_fault);
        locked_ = true;
        link_.invalidate();
        return false;
    }
    now_ = now;
    if (pending_ && now - pending_at_ >= staged_lifetime)
        clear_exchange();
    return true;
}
bool Simulator::healthy(uint64_t now) const {
    return sampled_ && active_ == 0 && now - sample_at_ <= period && !locked_;
}
void Simulator::sample(uint64_t now, uint32_t causes, bool pressed) {
    if (!clock(now))
        return;
    const bool gap = sampled_ && now - sample_at_ > period;
    const bool check = !sampled_ || now - sample_at_ >= period;
    const uint32_t newly_active = causes & ~active_;
    active_ = causes;
    if (newly_active)
        fault(newly_active);
    if (gap && (state_ == State::arm_pending || state_ == State::armed))
        fault(scheduling_fault);
    // Frequent calls do not manufacture ten healthy checks or move their baseline.
    if (check)
        sample_at_ = now;
    sampled_ = true;
    if (causes || locked_)
        return;
    if (state_ == State::boot)
        state_ = State::safe;
    if (!completed_)
        return;
    switch (button_) {
        case Button::released_needed:
            if (!pressed)
                button_ = Button::press;
            break;
        case Button::press:
            if (pressed) {
                button_ = Button::press_debounce;
                button_at_ = now;
            }
            break;
        case Button::press_debounce:
            if (!pressed)
                button_ = Button::press;
            else if (now - button_at_ >= debounce) {
                state_ = State::arm_pending;
                button_ = Button::release_debounce;
                button_at_ = now;
            }
            break;
        case Button::release_debounce:
            if (pressed)
                button_at_ = now;
            else {
                // Start release timing at the first observed released sample.
                if (was_pressed_)
                    button_at_ = now;
                if (now - button_at_ >= debounce) {
                    button_ = Button::qualify;
                    healthy_checks_ = 0;
                }
            }
            break;
        case Button::qualify:
            if (pressed)
                fault(scheduling_fault); // A new press invalidates this gesture.
            else if (state_ != State::armed && check && ++healthy_checks_ == 10)
                state_ = State::armed;
            break;
    }
    was_pressed_ = pressed;
}
Frame Simulator::respond(const Frame& request, Type type, Nak reason) {
    Frame r;
    r.type = type;
    r.epoch = epoch_;
    r.transaction = request.transaction;
    r.length = static_cast<uint8_t>(payload_size(type));
    if (type == Type::nak)
        put_le(r.payload.data(), static_cast<uint16_t>(reason), 2);
    else {
        const unsigned offset = type == Type::state_reply ? 8 : 0;
        if (offset)
            put_le(r.payload.data(), get_le(request.payload.data(), 8), 8);
        auto* p = r.payload.data() + offset;
        p[0] = static_cast<uint8_t>(state_);
        put_le(p + 1, active_, 4);
        put_le(p + 5, completed_ ? transaction_ : 0, 4);
        put_le(p + 9, completed_ ? epoch_ : 0, 4);
        put_le(p + 13, pl_session_, 4);
    }
    if (!link_.send(r)) {
        fault(scheduling_fault);
        return {};
    }
    link_.remember_reply(request, r);
    return r;
}
Frame Simulator::receive(const Frame& f, uint64_t now) {
    if (!clock(now))
        return {};
    auto event = link_.receive(f);
    if (event.delivery == Delivery::re_pair_required || event.delivery == Delivery::exhausted) {
        fault(scheduling_fault);
        return {};
    }
    if (event.respond)
        return event.response;
    if (event.delivery != Delivery::accepted)
        return {};
    if (f.type == Type::state_query)
        return respond(f, Type::state_reply);
    if (f.type == Type::recover_req) {
        if ((state_ != State::safe && state_ != State::fault) || !healthy(now))
            return respond(f, Type::nak, Nak::unhealthy);
        auto session = static_cast<uint32_t>(get_le(f.payload.data(), 4));
        if (!session || get_le(f.payload.data() + 4, 4) != cleanup_flags)
            return respond(f, Type::nak, Nak::invalid_attestation);
        if (!f.transaction || f.transaction <= last_transaction_)
            return respond(f, Type::nak, Nak::unknown_transaction);
        clear_exchange();
        transaction_ = last_transaction_ = f.transaction;
        pl_session_ = session;
        pending_ = true;
        pending_at_ = now;
        return respond(f, Type::recover_state);
    }
    if (f.type == Type::recover_commit) {
        if (!pending_ || f.transaction != transaction_ || f.epoch != epoch_)
            return respond(f, Type::nak, Nak::unknown_transaction);
        if (!healthy(now)) {
            fault(scheduling_fault);
            return respond(f, Type::nak, Nak::unhealthy);
        }
        pending_ = false;
        completed_ = true;
        state_ = State::safe;
        latched_ = 0;
        button_ = Button::released_needed;
        ++commits_;
        return respond(f, Type::commit_ack);
    }
    return {}; // Wrong-direction messages never mutate recovery state.
}
} // namespace monitor
} // namespace dmotor_sw
