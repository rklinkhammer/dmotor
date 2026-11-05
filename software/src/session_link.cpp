#include "dmotor_sw/session_link.hpp"
#include <algorithm>
namespace dmotor_sw {
namespace monitor {
SessionLink::SessionLink(uint32_t budget) : budget_(std::min(budget, UINT32_MAX - 1)) {}
void SessionLink::invalidate() {
    paired_ = false;
    discovering_ = false;
    // Keep lifetime high-water marks: invalidation does not permit ID reuse.
}
bool SessionLink::same(const Frame& a, const Frame& b) {
    return a.source == b.source && a.peer == b.peer && a.epoch == b.epoch &&
           a.transaction == b.transaction && a.sequence == b.sequence && a.type == b.type &&
           a.length == b.length &&
           std::equal(a.payload.begin(), a.payload.begin() + a.length, b.payload.begin());
}
bool SessionLink::number(Frame& frame) {
    if (tx_ >= budget_) {
        invalidate();
        return false;
    }
    frame.source = local_;
    frame.sequence = ++tx_;
    return true;
}
Frame SessionLink::begin(uint64_t fresh, bool inhibited) {
    invalidate();
    if (!inhibited || !fresh || fresh <= local_ || budget_ < 2)
        return {};
    local_ = fresh;
    peer_ = 0;
    tx_ = rx_ = 0;
    saw_hello_ = false;
    next_ = 0;
    history_ = {};
    hello_ = {};
    ack_ = {};
    discovering_ = true;
    Frame frame;
    frame.type = Type::hello;
    if (!number(frame))
        return {};
    return frame; // Retransmit this exact HELLO; do not call begin for a retry.
}
void SessionLink::cache(const Frame& frame) {
    history_[next_] = {true, false, frame, {}};
    next_ = (next_ + 1) % history_.size();
}
Reception SessionLink::replay(const Frame& frame) const {
    for (const auto& entry : history_)
        if (entry.used && entry.request.sequence == frame.sequence) {
            if (!same(entry.request, frame))
                return {};
            return {Delivery::duplicate, entry.has_reply, entry.reply};
        }
    return {}; // Eviction is rejection, never another execution.
}
Reception SessionLink::repair(uint64_t peer) {
    peer_floor_ = std::max(peer_floor_, peer);
    invalidate();
    return {Delivery::re_pair_required, false, {}};
}
Reception SessionLink::receive(const Frame& f) {
    if (!valid(f) || (!paired_ && !discovering_))
        return {};
    if (f.type == Type::hello) {
        if (f.source < peer_floor_)
            return {};
        if (paired_) {
            if (f.source > peer_)
                return repair(f.source);
            if (f.source != peer_)
                return {};
            if (saw_hello_ && same(hello_, f))
                return {Delivery::duplicate, true, ack_};
            return f.sequence > rx_ ? repair(f.source) : Reception{};
        }
        // While disabled/discovering, a newer peer lifetime may supersede a
        // stale candidate. Replies for the previous candidate cannot confirm it.
        if (saw_hello_ && f.source == peer_) {
            if (same(hello_, f))
                return {Delivery::duplicate, true, ack_};
            return f.sequence > rx_ ? repair(f.source) : Reception{};
        }
        if (peer_ && f.source < peer_)
            return {};
        if (f.sequence == UINT32_MAX) {
            invalidate();
            return {Delivery::exhausted, false, {}};
        }
        peer_ = f.source;
        peer_floor_ = std::max(peer_floor_, peer_);
        rx_ = f.sequence;
        history_ = {};
        next_ = 0;
        hello_ = f;
        saw_hello_ = true;
        ack_ = {};
        ack_.type = Type::hello_ack;
        ack_.peer = peer_;
        if (!number(ack_))
            return {Delivery::exhausted, false, {}};
        return {Delivery::discovery, true, ack_};
    }
    if (f.peer != local_ || !saw_hello_ || f.source != peer_)
        return {};
    if (f.type == Type::hello_ack) {
        if (f.epoch || f.transaction)
            return {};
        if (f.sequence <= rx_)
            return replay(f);
        if (f.sequence == UINT32_MAX) {
            invalidate();
            return {Delivery::exhausted, false, {}};
        }
        // A delayed second ACK must not advance an established control stream.
        if (paired_)
            return {};
        rx_ = f.sequence;
        cache(f);
        paired_ = true;
        discovering_ = false;
        return {Delivery::paired, false, {}};
    }
    if (!paired_)
        return {};
    if (f.sequence <= rx_)
        return replay(f);
    if (f.sequence == UINT32_MAX) {
        invalidate();
        return {Delivery::exhausted, false, {}};
    }
    rx_ = f.sequence;
    cache(f);
    return {Delivery::accepted, false, {}};
}
bool SessionLink::send(Frame& frame) {
    if (!paired_ || frame.type == Type::hello || frame.type == Type::hello_ack)
        return false;
    Frame candidate = frame;
    candidate.source = local_;
    candidate.peer = peer_;
    candidate.sequence = 1;
    if (!valid(candidate))
        return false;
    if (!number(candidate))
        return false;
    frame = candidate;
    return true;
}
bool SessionLink::remember_reply(const Frame& request, const Frame& response) {
    if (!paired_ || !valid(request) || !valid(response) || response.source != local_ ||
        response.peer != peer_ || response.sequence > tx_ || response.type == Type::hello ||
        response.type == Type::hello_ack)
        return false;
    for (auto& entry : history_)
        if (entry.used && same(entry.request, request)) {
            if (entry.has_reply)
                return same(entry.reply, response); // Immutable cached outcome.
            entry.reply = response;
            entry.has_reply = true;
            return true;
        }
    return false;
}
} // namespace monitor
} // namespace dmotor_sw
