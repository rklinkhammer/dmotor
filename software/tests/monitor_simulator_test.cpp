#include "support/monitor_simulator.hpp"
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
Frame wire(const Frame& f) {
    std::array<uint8_t, max_encoded> bytes{};
    auto size = encode(f, bytes);
    CHECK(size);
    Decoder d;
    Frame out;
    Decode result = Decode::incomplete;
    for (std::size_t i = 0; i < size; ++i)
        result = d.push(bytes[i], out);
    CHECK(result == Decode::frame);
    return out;
}
const recovery::Cleanup proof{7, 8, 9, true, true, true};
struct Joint {
    recovery::Link ps;
    Simulator mon;
    Joint(bool held = false) {
        auto ph = ps.discover(11, true), mh = mon.boot(22);
        mon.sample(0, 0, held);
        auto ma = mon.receive(wire(ph), 0);
        auto pa = ps.receive(wire(mh), 0);
        ps.receive(wire(ma), 0);
        mon.receive(wire(pa.response), 0);
        CHECK(ps.begin(proof, 0));
    }
    Frame commit(bool deliver = true) {
        auto req = ps.poll(0);
        ps.receive(wire(mon.receive(wire(req), 0)), 1);
        auto c = ps.poll(1);
        auto ack = mon.receive(wire(c), 1);
        CHECK(ack.type == Type::commit_ack && mon.commits() == 1);
        if (deliver)
            ps.receive(wire(ack), 2);
        return c;
    }
    void ticks(unsigned first, unsigned last, bool pressed, uint32_t causes = 0) {
        for (unsigned i = first; i <= last; ++i)
            mon.sample(uint64_t(i) * 10000, causes, pressed);
    }
    void arm() {
        ticks(1, 1, false); // Released observation after commit.
        ticks(2, 4, true);  // Press stable 20 ms.
        CHECK(mon.state() == State::arm_pending && !mon.permits());
        ticks(5, 7, false); // Release stable 20 ms.
        ticks(8, 16, false);
        CHECK(!mon.permits()); // Nine checks cannot arm.
        ticks(17, 17, false);
        CHECK(mon.permits());
    }
};
struct Raw {
    SessionLink ps;
    Simulator mon;
    Raw() {
        auto ph = ps.begin(11, true), mh = mon.boot(22);
        mon.sample(0, 0, false);
        auto ma = mon.receive(wire(ph), 0);
        auto pa = ps.receive(wire(mh));
        CHECK(ps.receive(wire(ma)).delivery == Delivery::paired);
        mon.receive(wire(pa.response), 0);
    }
    Frame request(uint32_t txn = 1) {
        Frame f;
        f.type = Type::recover_req;
        f.transaction = txn;
        f.length = 8;
        put_le(f.payload.data(), 7, 4);
        put_le(f.payload.data() + 4, 7, 4);
        CHECK(ps.send(f));
        return f;
    }
    Frame commit(uint32_t txn = 1, uint32_t epoch = 0) {
        Frame f;
        f.type = Type::recover_commit;
        f.transaction = txn;
        f.epoch = epoch;
        CHECK(ps.send(f));
        return f;
    }
};
int main() {
    for (bool lost : {false, true}) {
        Joint j;
        auto c = j.commit(!lost);
        j.arm();
        CHECK(!j.ps.admission_open());
        if (lost) {
            // Host scheduling is independent; retries carry original COMMIT identity.
            auto retry = j.ps.poll(100001);
            auto ack = j.mon.receive(wire(retry), 170001);
            CHECK(ack.type == Type::commit_ack && j.mon.commits() == 1);
            j.ps.poll(200001);
            j.ticks(18, 30, false);
        }
        const uint64_t now = lost ? 300001 : 170001;
        auto q = j.ps.poll(now);
        CHECK(q.type == Type::state_query);
        auto r = j.mon.receive(wire(q), now);
        CHECK(r.payload[8] == static_cast<uint8_t>(State::armed));
        j.ps.receive(wire(r), now + 1);
        CHECK(!j.ps.admission_open());
        CHECK(j.ps.confirm_local(proof, now + 2));
        CHECK(j.ps.admission_open());
        j.mon.sample(now + 3, 1, false);
        CHECK(!j.mon.permits() && j.mon.epoch() == 1);
        // Cached COMMIT_ACK must not clear a newer fault or execute COMMIT again.
        j.mon.receive(wire(c), now + 4);
        CHECK(j.mon.state() == State::fault && j.mon.commits() == 1);
        auto hello = j.mon.boot(23);
        CHECK(!j.mon.permits());
        j.ps.receive(wire(hello), now + 5);
        CHECK(!j.ps.admission_open() && j.ps.halt_required());
    }
    {
        Joint j(true);
        j.commit();
        j.ticks(1, 30, true);
        CHECK(!j.mon.permits() && j.mon.state() == State::safe);
        j.ticks(31, 31, false);
        j.ticks(32, 34, true);
        j.ticks(35, 47, false);
        CHECK(j.mon.permits());
    }
    {
        Joint j;
        j.commit();
        j.ticks(1, 1, false);
        j.ticks(2, 2, true);
        j.ticks(3, 3, false);
        CHECK(j.mon.state() == State::safe); // Press bounce.
        j.ticks(4, 6, true);
        j.ticks(7, 7, false);
        j.ticks(8, 8, true);
        j.ticks(9, 11, false);
        j.ticks(12, 20, false);
        CHECK(!j.mon.permits());
        j.ticks(21, 21, false);
        CHECK(j.mon.permits());
    }
    {
        Joint j;
        j.commit();
        j.ticks(1, 1, false);
        j.ticks(2, 4, true);
        j.ticks(5, 7, false);
        for (unsigned i = 0; i < 100; ++i)
            j.mon.sample(70000, 0, false);
        CHECK(!j.mon.permits()); // Repeated samples are not elapsed 10 ms checks.
        j.ticks(8, 16, false);
        CHECK(!j.mon.permits());
        j.ticks(17, 17, false);
        CHECK(j.mon.permits());
    }
    for (unsigned boundary : {4U, 7U, 17U}) {
        Joint j;
        j.commit();
        for (unsigned i = 1; i <= boundary; ++i) {
            bool pressed = i >= 2 && i <= 4;
            j.mon.sample(uint64_t(i) * 10000, i == boundary ? 1 : 0, pressed);
        }
        CHECK(!j.mon.permits() && j.mon.state() == State::fault);
    }
    {
        Raw r;
        auto req = r.request();
        CHECK(r.mon.receive(wire(req), 0).type == Type::recover_state);
        // Fault sampled at the COMMIT timestamp wins. Repeated level is one epoch.
        r.mon.sample(1, 1, false);
        r.mon.sample(2, 1, false);
        CHECK(r.mon.epoch() == 1);
        CHECK(r.mon.receive(wire(r.commit()), 2).type == Type::nak);
        r.mon.sample(3, 3, false);
        CHECK(r.mon.epoch() == 2);
        r.mon.sample(4, 0, false);
        r.mon.sample(5, 1, false);
        CHECK(r.mon.epoch() == 3 && r.mon.latched_causes() == 3);
        r.mon.sample(6, 0, false);
        auto req2 = r.request(2);
        CHECK(r.mon.receive(wire(req2), 6).type == Type::recover_state);
        CHECK(r.mon.receive(wire(r.commit(2, 3)), 6).type == Type::commit_ack);
        CHECK(r.mon.state() == State::safe && !r.mon.permits());
    }
    for (uint64_t deadline : {299999ULL, 300000ULL}) {
        Raw r;
        auto req = r.request();
        r.mon.receive(wire(req), 0);
        r.mon.sample(200000, 0, false);
        r.mon.receive(wire(req), 200000); // Retry never renews staged expiry.
        r.mon.sample(deadline, 0, false);
        auto result = r.mon.receive(wire(r.commit()), deadline);
        CHECK(result.type == (deadline == 300000 ? Type::nak : Type::commit_ack));
    }
    {
        Raw r;
        r.mon.receive(wire(r.request()), 0);
        r.mon.receive(wire(r.request(2)), 1); // Superseding request invalidates old commit.
        CHECK(r.mon.receive(wire(r.commit()), 2).type == Type::nak);
        CHECK(r.mon.receive(wire(r.commit(2)), 3).type == Type::commit_ack);
    }
    {
        Raw r;
        auto req = r.request();
        put_le(req.payload.data() + 4, 3, 4);
        CHECK(r.mon.receive(wire(req), 0).type == Type::nak);
        auto good = r.request(2);
        r.mon.receive(wire(good), 0);
        CHECK(r.mon.receive(wire(r.commit(2)), 10001).type == Type::nak); // Stale health sample.
    }
    {
        Joint j;
        j.commit();
        j.ticks(1, 1, false);
        j.ticks(2, 4, true);
        j.mon.sample(60000, 0, false); // Missed 10 ms check during ARM_PENDING.
        CHECK(!j.mon.permits() && j.mon.state() == State::fault);
        j.mon.sample(1, 0, false); // Backward time locks this boot.
        CHECK(!j.mon.permits());
        CHECK(!valid(j.mon.boot(22))); // Lifetime reuse refused.
        CHECK(valid(j.mon.boot(23)) && !j.mon.permits());
    }
    std::cout
        << "monitor simulator PASS: fault epochs, commit/expiry races, held/bouncing ARM, ten checks, lost ACK, reset and admission\n";
}
