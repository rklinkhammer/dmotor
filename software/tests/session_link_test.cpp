#include "dmotor_sw/session_link.hpp"
#include <cstdlib>
#include <iostream>
using namespace dmotor_sw::monitor;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << __LINE__ << ": " #x "\n";                                                 \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
Reception wire(SessionLink& link, const Frame& frame) {
    std::array<uint8_t, max_encoded> bytes{};
    auto n = encode(frame, bytes);
    CHECK(n);
    Decoder decoder;
    Frame decoded;
    Decode state = Decode::incomplete;
    for (std::size_t i = 0; i < n; ++i)
        state = decoder.push(bytes[i], decoded);
    CHECK(state == Decode::frame);
    return link.receive(decoded);
}
struct Handshake {
    Frame ah, bh, aa, ba;
};
Handshake pair_links(SessionLink& a, SessionLink& b, uint64_t aid = 11, uint64_t bid = 22,
                     bool reverse = false) {
    Handshake h;
    h.ah = a.begin(aid, true);
    h.bh = b.begin(bid, true);
    CHECK(!a.paired() && !b.paired());
    if (reverse) {
        h.aa = wire(a, h.bh).response;
        h.ba = wire(b, h.ah).response;
        CHECK(wire(b, h.aa).delivery == Delivery::paired);
        CHECK(!a.paired());
        CHECK(wire(a, h.ba).delivery == Delivery::paired);
    } else {
        h.ba = wire(b, h.ah).response;
        h.aa = wire(a, h.bh).response;
        CHECK(wire(a, h.ba).delivery == Delivery::paired);
        CHECK(!b.paired());
        CHECK(wire(b, h.aa).delivery == Delivery::paired);
    }
    CHECK(a.paired() && b.paired());
    CHECK(a.peer_session() == bid && b.peer_session() == aid);
    return h;
}
Frame query(SessionLink& link, uint32_t txn = 7) {
    Frame f;
    f.type = Type::state_query;
    f.epoch = 3;
    f.transaction = txn;
    f.length = 8;
    put_le(f.payload.data(), 0x7e7d123456ULL, 8);
    CHECK(link.send(f));
    return f;
}
int main() {
    for (bool reverse : {false, true}) {
        SessionLink a, b;
        auto h = pair_links(a, b, 11, 22, reverse);
        auto request = query(a);
        CHECK(wire(b, request).delivery == Delivery::accepted);
        auto duplicate = wire(b, request);
        CHECK(duplicate.delivery == Delivery::duplicate && !duplicate.respond);
        Frame answer;
        answer.type = Type::state_reply;
        answer.epoch = 3;
        answer.transaction = 7;
        answer.length = 25;
        put_le(answer.payload.data(), 0x7e7d123456ULL, 8);
        answer.payload[8] = static_cast<uint8_t>(State::safe);
        CHECK(b.send(answer));
        CHECK(b.remember_reply(request, answer));
        CHECK(b.remember_reply(request, answer));
        duplicate = wire(b, request);
        CHECK(duplicate.delivery == Delivery::duplicate && duplicate.respond);
        CHECK(duplicate.response.sequence == answer.sequence &&
              duplicate.response.payload == answer.payload);
        CHECK(wire(a, answer).delivery == Delivery::accepted);
        CHECK(wire(a, answer).delivery == Delivery::duplicate);
        auto changed = request;
        changed.payload[0] ^= 1;
        CHECK(wire(b, changed).delivery == Delivery::rejected);
        auto changed_answer = answer;
        changed_answer.payload[8] = static_cast<uint8_t>(State::armed);
        CHECK(!b.remember_reply(request, changed_answer));
        // Eviction may lose the reply, but never allows another mutation.
        for (unsigned i = 0; i < 6; ++i)
            CHECK(wire(b, query(a, 8 + i)).delivery == Delivery::accepted);
        CHECK(wire(b, request).delivery == Delivery::rejected);
        CHECK(!b.remember_reply(request, answer));
        // Discovery replay remains recognizable even after control cache eviction.
        auto hello_again = wire(b, h.ah);
        CHECK(hello_again.delivery == Delivery::duplicate && hello_again.respond);
        CHECK(hello_again.response.sequence == h.ba.sequence && b.paired());
        auto bad = query(a);
        bad.peer++;
        CHECK(wire(b, bad).delivery == Delivery::rejected);
        bad.peer--;
        bad.source++;
        CHECK(wire(b, bad).delivery == Delivery::rejected);
        CHECK(b.paired());
    }
    {
        // ACK-before-HELLO is ignored; lost HELLO/ACK are recovered by exact retries.
        SessionLink a, b;
        auto ah = a.begin(11, true), bh = b.begin(22, true);
        auto ba = wire(b, ah);
        CHECK(!b.paired());
        CHECK(wire(a, ba.response).delivery == Delivery::rejected);
        auto aa = wire(a, bh);
        CHECK(wire(b, aa.response).delivery == Delivery::paired);
        auto ba_retry = wire(b, ah);
        CHECK(ba_retry.delivery == Delivery::duplicate);
        CHECK(wire(a, ba_retry.response).delivery == Delivery::paired);
    }
    {
        SessionLink a, b;
        auto old = pair_links(a, b);
        // Restart must be discovered even when HB never expired. It closes link
        // acceptance, but cannot replace pairing or execute any control frame.
        auto restarted = a.begin(12, true);
        CHECK(wire(b, restarted).delivery == Delivery::re_pair_required);
        CHECK(!b.paired() && b.peer_session() == 0);
        CHECK(wire(b, old.ah).delivery == Delivery::rejected);
        auto bh = b.begin(23, true);
        auto aa = wire(a, bh);
        auto ba = wire(b, restarted);
        CHECK(wire(a, old.ba).delivery == Delivery::rejected); // Echoes old local ID.
        CHECK(wire(a, ba.response).delivery == Delivery::paired);
        CHECK(wire(b, aa.response).delivery == Delivery::paired);
        auto stale = old.ah;
        CHECK(wire(b, stale).delivery == Delivery::rejected);
        CHECK(b.paired() && b.peer_session() == 12);
        auto explicit_hello = restarted;
        explicit_hello.sequence = 100;
        CHECK(wire(b, explicit_hello).delivery == Delivery::re_pair_required);
        CHECK(!b.paired());
        CHECK(!valid(b.begin(23, true))); // Same local lifetime cannot restart pairing.
        CHECK(!valid(a.begin(13, false)));
        CHECK(!a.paired());
    }
    {
        // Stale discovery candidate can be superseded while still inhibited.
        SessionLink a;
        auto ah = a.begin(11, true);
        CHECK(valid(ah));
        Frame hello;
        hello.type = Type::hello;
        hello.source = 21;
        hello.sequence = 1;
        auto old_ack = wire(a, hello);
        CHECK(old_ack.respond);
        hello.source = 22;
        CHECK(wire(a, hello).respond);
        Frame ack;
        ack.type = Type::hello_ack;
        ack.source = 21;
        ack.peer = 11;
        ack.sequence = 2;
        CHECK(wire(a, ack).delivery == Delivery::rejected);
        ack.source = 22;
        CHECK(wire(a, ack).delivery == Delivery::paired);
    }
    {
        SessionLink a(3), b;
        pair_links(a, b);
        auto last = query(a);
        CHECK(last.sequence == 3);
        Frame next;
        next.type = Type::state_query;
        next.length = 8;
        CHECK(!a.send(next) && !a.paired());
        auto max = last;
        max.sequence = UINT32_MAX;
        CHECK(wire(b, max).delivery == Delivery::exhausted && !b.paired());
        CHECK(wire(b, last).delivery == Delivery::rejected);
    }
    {
        SessionLink a, b;
        CHECK(!valid(a.begin(0, true)));
        auto ah = a.begin(11, true), bh = b.begin(22, true);
        auto bad = bh;
        bad.length = 65;
        CHECK(a.receive(bad).delivery == Delivery::rejected);
        Frame control;
        control.type = Type::recover_commit;
        control.source = 22;
        control.peer = 11;
        control.sequence = 2;
        CHECK(wire(a, control).delivery == Delivery::rejected);
        auto aa = wire(a, bh), ba = wire(b, ah);
        auto ack = ba.response;
        ack.epoch = 1;
        CHECK(wire(a, ack).delivery == Delivery::rejected && !a.paired());
        CHECK(wire(a, ba.response).delivery == Delivery::paired);
        CHECK(wire(b, aa.response).delivery == Delivery::paired);
        auto req = query(a);
        auto changed = req;
        changed.sequence++;
        changed.length = 7;
        CHECK(b.receive(changed).delivery == Delivery::rejected);
        CHECK(wire(b, req).delivery == Delivery::accepted); // Malformed frame did not advance RX.
        auto replay_ack = ba.response;
        replay_ack.sequence = 1000;
        CHECK(wire(a, replay_ack).delivery == Delivery::rejected);
        CHECK(a.paired());
    }
    std::cout
        << "session link PASS: symmetric discovery, retries, restart, lifetime echoes, immutable duplicates, eviction, malformed frames and exhaustion\n";
}
