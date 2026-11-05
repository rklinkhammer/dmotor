#include "dmotor_sw/recovery_link.hpp"
#include <cstdlib>
#include <iostream>
using namespace dmotor_sw;
using namespace dmotor_sw::monitor;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << __LINE__ << ": " #x "\n";                                                 \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
const recovery::Cleanup proof{7, 8, 9, true, true, true};
Frame wire(const Frame& f) {
    std::array<uint8_t, max_encoded> bytes{};
    auto size = encode(f, bytes);
    CHECK(size);
    Decoder decoder;
    Frame decoded;
    Decode result = Decode::incomplete;
    for (std::size_t i = 0; i < size; ++i)
        result = decoder.push(bytes[i], decoded);
    CHECK(result == Decode::frame);
    return decoded;
}
bool identical(const Frame& a, const Frame& b) {
    std::array<uint8_t, max_encoded> x{}, y{};
    auto nx = encode(a, x), ny = encode(b, y);
    return nx && nx == ny && x == y;
}
// Scripted monitor fixture, not ESP32 safety/ARM firmware. Real link/codec,
// deterministic transaction completion, and explicit simulated physical ARM.
struct Fixture {
    recovery::Link ps;
    SessionLink mon;
    unsigned commits = 0;
    uint32_t txn = 0;
    State state = State::fault;
    Fixture() {
        auto ph = ps.discover(11, true), mh = mon.begin(22, true);
        auto ma = mon.receive(wire(ph));
        auto pa = ps.receive(wire(mh), 0);
        CHECK(ps.receive(wire(ma.response), 0).delivery == Delivery::paired);
        CHECK(mon.receive(wire(pa.response)).delivery == Delivery::paired);
        CHECK(ps.begin(proof, 0));
    }
    Frame answer(const Frame& request) {
        auto f = wire(request);
        auto received = mon.receive(f);
        if (received.delivery == Delivery::duplicate) {
            CHECK(received.respond);
            return received.response;
        }
        CHECK(received.delivery == Delivery::accepted);
        Frame r;
        r.transaction = f.transaction;
        r.epoch = 3;
        if (f.type == Type::recover_req) {
            CHECK(get_le(f.payload.data(), 4) == proof.session);
            CHECK(get_le(f.payload.data() + 4, 4) == cleanup_flags);
            txn = f.transaction;
            r.type = Type::recover_state;
            r.length = 17;
        } else if (f.type == Type::recover_commit) {
            CHECK(f.transaction == txn && f.epoch == 3);
            ++commits;
            state = State::safe;
            r.type = Type::commit_ack;
            r.length = 17;
        } else {
            CHECK(f.type == Type::state_query && f.transaction == txn && f.epoch == 3);
            r.type = Type::state_reply;
            r.length = 25;
            put_le(r.payload.data(), get_le(f.payload.data(), 8), 8);
        }
        auto* p = r.payload.data() + (r.type == Type::state_reply ? 8 : 0);
        p[0] = static_cast<uint8_t>(state);
        put_le(p + 1, 0, 4);
        put_le(p + 5, commits ? txn : 0, 4);
        put_le(p + 9, commits ? 3 : 0, 4);
        put_le(p + 13, proof.session, 4);
        CHECK(mon.send(r));
        CHECK(mon.remember_reply(f, r));
        return r;
    }
    Frame commit() {
        auto req = ps.poll(0);
        CHECK(req.sequence == 3); // Discovery consumed wire 1/2; logical request is 1.
        auto response = answer(req);
        CHECK(ps.receive(wire(response), 1).delivery == Delivery::accepted);
        CHECK(ps.receive(wire(response), 1).delivery == Delivery::duplicate);
        return ps.poll(1);
    }
};
int main() {
    for (bool lost_ack : {false, true}) {
        Fixture f;
        auto commit = f.commit();
        auto ack = f.answer(commit);
        uint64_t now = 2;
        if (lost_ack) {
            for (uint64_t t : {100001ULL, 200001ULL}) {
                auto retry = f.ps.poll(t);
                CHECK(identical(commit, retry));
                CHECK(identical(ack, f.answer(retry)));
            }
            now = 300001;
        } else
            f.ps.receive(wire(ack), now);
        CHECK(f.commits == 1 && !f.ps.admission_open());
        auto q = f.ps.poll(now);
        CHECK(q.type == Type::state_query);
        auto safe = f.answer(q);
        f.ps.receive(wire(safe), ++now);
        CHECK(f.ps.phase() == recovery::Phase::waiting_arm);
        CHECK(!f.ps.confirm_local(proof, ++now));
        f.state = State::armed; // Explicit fixture event, never inferred from UART.
        CHECK(f.ps.query_arm(++now));
        auto fresh = f.ps.poll(now);
        CHECK(get_le(fresh.payload.data(), 8) != get_le(q.payload.data(), 8));
        f.ps.receive(wire(safe), ++now);
        CHECK(!f.ps.admission_open());
        f.ps.receive(wire(f.answer(fresh)), ++now);
        CHECK(f.ps.phase() == recovery::Phase::local_check && !f.ps.admission_open());
        CHECK(f.ps.confirm_local(proof, ++now));
        CHECK(f.ps.admission_open());
        auto restarted = f.mon.begin(23, true);
        CHECK(f.ps.receive(wire(restarted), ++now).delivery == Delivery::re_pair_required);
        CHECK(!f.ps.admission_open() && f.ps.halt_required());
        CHECK(!valid(f.ps.poll(++now)));
        CHECK(!f.ps.confirm_local(proof, ++now));
    }
    {
        Fixture f;
        auto req = f.ps.poll(0);
        CHECK(identical(req, f.ps.poll(100000)));
        CHECK(identical(req, f.ps.poll(200000)));
        CHECK(!valid(f.ps.poll(300000)));
        CHECK(f.ps.phase() == recovery::Phase::failed && f.ps.halt_required());
    }
    {
        Fixture f;
        f.answer(f.commit()); // Drop all COMMIT_ACKs and query replies.
        f.ps.poll(100001);
        f.ps.poll(200001);
        auto query = f.ps.poll(300001);
        CHECK(identical(query, f.ps.poll(400001)));
        CHECK(identical(query, f.ps.poll(500001)));
        CHECK(!valid(f.ps.poll(600001)));
        CHECK(f.ps.phase() == recovery::Phase::unknown && f.ps.halt_required());
        f.state = State::armed;
        f.ps.receive(wire(f.answer(query)), 600002);
        CHECK(!f.ps.admission_open());
    }
    {
        Fixture f;
        auto ack = f.answer(f.commit());
        f.ps.receive(wire(ack), 2);
        auto q = f.ps.poll(2);
        f.state = State::armed;
        auto stale = f.answer(q);
        stale.payload[0] ^= 1; // Correct wire identity, wrong challenge.
        f.ps.receive(wire(stale), 3);
        CHECK(f.ps.phase() == recovery::Phase::query && !f.ps.admission_open());
        auto good = stale;
        good.payload[0] ^= 1;
        CHECK(f.mon.send(good)); // Fresh sequence, exact current challenge.
        f.ps.receive(wire(good), 4);
        CHECK(f.ps.phase() == recovery::Phase::local_check);
        auto changed = proof;
        ++changed.configuration;
        CHECK(!f.ps.confirm_local(changed, 5));
        CHECK(f.ps.halt_required() && !f.ps.admission_open());
    }
    for (unsigned bad = 0; bad < 5; ++bad) {
        Fixture f;
        auto ack = f.answer(f.commit());
        if (bad == 0)
            ack.payload[0] = 255;
        if (bad == 1)
            ack.epoch++;
        if (bad == 2) {
            ack.type = Type::fault_evt;
            ack.length = 4;
        }
        if (bad == 3) {
            ack.type = Type::nak;
            ack.length = 2;
        }
        if (bad == 4)
            ack.peer++;
        f.ps.receive(wire(ack), 2);
        CHECK(!f.ps.admission_open());
        if (bad != 4)
            CHECK(f.ps.halt_required());
        f.ps.invalidate();
        CHECK(f.ps.halt_required() && !valid(f.ps.poll(3)));
    }
    std::cout
        << "recovery link PASS: wire identity, exact retries, single commit, lost ACK, challenged ARM, final proof, restart and uncertainty\n";
}
