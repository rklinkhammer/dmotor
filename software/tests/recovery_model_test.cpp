#include "dmotor_sw/recovery_model.hpp"
#include <cstdlib>
#include <iostream>
using namespace dmotor_sw;
using namespace dmotor_sw::recovery;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << __LINE__ << ": " #x "\n";                                                 \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
namespace {
const Pair pair{11, 22};
const Cleanup proof{7, 8, 9, true, true, true};
Reply reply(uint32_t txn = 1) {
    Reply r;
    r.pair = pair;
    r.transaction = txn;
    r.epoch = 3;
    r.state = monitor::State::safe;
    r.completed_transaction = txn;
    r.completed_epoch = 3;
    r.pl_session = proof.session;
    return r;
}
void held(const Model& m) {
    CHECK(!m.admission_open());
    for (auto op : {Operation::enable, Operation::velocity, Operation::duty, Operation::stop,
                    Operation::timeout_injection})
        CHECK(!m.allowed(op));
    for (auto op : {Operation::ping, Operation::status, Operation::capture, Operation::disable,
                    Operation::halt, Operation::quit})
        CHECK(m.allowed(op));
}
void start_commit(Model& m) {
    CHECK(m.begin(proof, 0));
    held(m);
    CHECK(!m.begin(proof, 0)); // Never replace an in-flight exchange.
    CHECK(m.poll(0).message == Message::recover_request);
    CHECK(m.recovered(reply(), 1));
    CHECK(m.poll(1).message == Message::recover_commit);
    held(m);
}
Reply armed_query(Model& m) {
    CHECK(m.committed(reply(), 2));
    held(m);
    auto tx = m.poll(2);
    CHECK(tx.message == Message::state_query);
    auto r = reply();
    r.challenge = tx.challenge;
    r.state = monitor::State::armed;
    return r;
}
} // namespace
int main() {
    {
        Model m(pair);
        held(m);
        CHECK(m.halt_required());
        CHECK(!m.recovered(reply(), 0));
        start_commit(m);
        auto r = armed_query(m);
        CHECK(m.state_reply(r, 3));
        held(m);
        CHECK(m.confirm_local(proof, 4));
        CHECK(m.admission_open());
        CHECK(m.allowed(Operation::enable));
        CHECK(m.poll(1000000).message == Message::none); // Advisory UART loss after completion.
        CHECK(m.admission_open());
        m.abort();
        held(m);
        CHECK(m.halt_required());
        CHECK(!m.confirm_local(proof, 1000001));
        CHECK(!m.begin(proof, 1000002));
    }
    // A lost COMMIT_ACK may already have committed/armed remotely. Query the
    // exact tuple, never issue another RECOVER_REQ or assume rollback.
    for (auto state : {monitor::State::safe, monitor::State::arm_pending, monitor::State::armed}) {
        Model m(pair);
        start_commit(m);
        auto retry = m.poll(100001);
        auto again = m.poll(200001);
        CHECK(retry.message == Message::recover_commit && retry.sequence == again.sequence);
        CHECK(retry.transaction == again.transaction && retry.epoch == again.epoch);
        auto q = m.poll(300001);
        CHECK(q.message == Message::state_query);
        held(m);
        auto r = reply();
        r.challenge = q.challenge;
        r.state = state;
        CHECK(m.state_reply(r, 300002));
        held(m);
        if (state != monitor::State::armed) {
            CHECK(m.phase() == Phase::waiting_arm);
            CHECK(!m.confirm_local(proof, 300003));
            CHECK(m.query_arm(400000));
            auto fresh = m.poll(400000);
            CHECK(fresh.challenge != q.challenge);
            r.state = monitor::State::armed;
            CHECK(!m.state_reply(r, 400001)); // Old ARMED challenge is not freshness.
            r.challenge = fresh.challenge;
            CHECK(m.state_reply(r, 400002));
        }
        CHECK(m.confirm_local(proof, state == monitor::State::armed ? 300003 : 400003));
        CHECK(m.admission_open());
    }
    {
        Model m(pair);
        start_commit(m);
        m.poll(100001);
        m.poll(200001);
        auto q = m.poll(300001);
        auto q2 = m.poll(400001);
        auto q3 = m.poll(500001);
        CHECK(q.challenge == q2.challenge && q.sequence == q3.sequence);
        CHECK(m.poll(600001).message == Message::none);
        CHECK(m.phase() == Phase::unknown && m.halt_required());
        held(m);
        auto late = reply();
        late.challenge = q.challenge;
        late.state = monitor::State::armed;
        CHECK(!m.state_reply(late, 600002));
        held(m);
    }
    // Fault, changed tuple/epoch or failed final local proof cannot open admission.
    for (unsigned fault = 0; fault < 9; ++fault) {
        Model m(pair);
        start_commit(m);
        auto r = armed_query(m);
        if (fault < 4) {
            if (fault == 0)
                ++r.epoch;
            if (fault == 1)
                ++r.completed_epoch;
            if (fault == 2)
                ++r.completed_transaction;
            if (fault == 3)
                r.causes = 1;
            CHECK(!m.state_reply(r, 3));
        } else {
            CHECK(m.state_reply(r, 3));
            auto bad = proof;
            if (fault == 4)
                bad.disabled = false;
            if (fault == 5)
                bad.drained = false;
            if (fault == 6)
                bad.causes_clear = false;
            if (fault == 7)
                ++bad.configuration;
            if (fault == 8)
                ++bad.request;
            CHECK(!m.confirm_local(bad, 4));
        }
        held(m);
        CHECK(m.halt_required());
    }
    {
        Model m(pair);
        start_commit(m);
        auto r = armed_query(m);
        ++r.pair.monitor;
        CHECK(!m.state_reply(r, 3));
        held(m);
        --r.pair.monitor;
        ++r.transaction;
        CHECK(!m.state_reply(r, 4));
        held(m);
        m.abort();
        --r.transaction;
        CHECK(!m.state_reply(r, 5));
    }
    {
        Model m(pair);
        CHECK(m.begin(proof, 0));
        auto a = m.poll(0);
        CHECK(m.poll(99999).message == Message::none);
        CHECK(!m.recovered(reply(), 100000)); // Expiry wins at boundary.
        auto b = m.poll(100000);
        CHECK(a.sequence == b.sequence && a.cleanup.request == b.cleanup.request);
        m.poll(200000);
        m.poll(300000);
        held(m);
        CHECK(m.phase() == Phase::failed && m.halt_required());
    }
    {
        Model m(pair);
        start_commit(m);
        auto r = armed_query(m);
        CHECK(m.state_reply(r, 3));
        CHECK(!m.confirm_local(proof, 100003));
        held(m);
        CHECK(m.halt_required());
        Model backwards(pair);
        CHECK(backwards.begin(proof, 10));
        backwards.poll(9);
        CHECK(backwards.halt_required());
        held(backwards);
        Model exhausted(pair);
        CHECK(!exhausted.begin(proof, UINT64_MAX));
        held(exhausted);
        Model unpaired({0, 22});
        CHECK(!unpaired.begin(proof, 0));
        held(unpaired);
    }
    // Epoch zero is legal before a monitor has observed its first fault.
    {
        Model m(pair);
        CHECK(m.begin(proof, 0));
        m.poll(0);
        auto r = reply();
        r.epoch = r.completed_epoch = 0;
        CHECK(m.recovered(r, 1));
        m.poll(1);
        CHECK(m.committed(r, 2));
        r.challenge = m.poll(2).challenge;
        r.state = monitor::State::armed;
        CHECK(m.state_reply(r, 3));
        CHECK(m.confirm_local(proof, 4));
    }
    // A detected fault/restart/ownership loss wins in every pre-admission phase.
    for (unsigned phase = 0; phase < 4; ++phase) {
        Model m(pair);
        CHECK(m.begin(proof, 0));
        m.poll(0);
        auto r = reply();
        if (phase > 0) {
            CHECK(m.recovered(r, 1));
            m.poll(1);
        }
        if (phase > 1) {
            CHECK(m.committed(r, 2));
            r.challenge = m.poll(2).challenge;
        }
        if (phase > 2) {
            r.state = monitor::State::armed;
            CHECK(m.state_reply(r, 3));
        }
        m.abort();
        held(m);
        CHECK(m.halt_required());
        CHECK(!m.confirm_local(proof, 4));
        CHECK(!m.state_reply(r, 4));
    }
    for (unsigned field = 0; field < 6; ++field) {
        auto bad = proof;
        if (field == 0)
            bad.session = 0;
        if (field == 1)
            bad.request = 0;
        if (field == 2)
            bad.configuration = 0;
        if (field == 3)
            bad.disabled = false;
        if (field == 4)
            bad.drained = false;
        if (field == 5)
            bad.causes_clear = false;
        Model m(pair);
        CHECK(!m.begin(bad, 0));
        held(m);
    }
    std::cout
        << "recovery model PASS: admission hold, challenged ARM, lost ACK, retries, uncertainty, identity, faults and local recheck\n";
}
