#include "dmotor_sw/runtime.hpp"
#include "dmotor_sw/version.hpp"
#include "dmotor/controller_registers.hpp"
#include <map>
#include <vector>
#include <iostream>
#include <cstdlib>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << __LINE__ << ": " #x "\n";                                                 \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
using namespace dmotor_sw;
namespace reg = dmotor::controller_reg;
namespace b = dmotor::batch_reg;
#include "support/runtime_fixture.hpp"
int main(int argc, char** argv) {
    if (startup_version("dmotor-runtime-tests", argc, argv))
        return 0;
    {
        Fake io;
        RuntimePolicy p;
        p.recovery_required = true;
        p.commissioning = true;
        Runtime r(io, p);
        CHECK(r.recovery_held());
        r.prepare(config(), 1);
        const auto first = r.recovery_generation();
        const auto writes = io.writes.size();
        CHECK(rejects([&] { r.enable(1); }));
        CHECK(rejects([&] { r.velocity(1, {{1000, 0, 0, 0}}); }));
        CHECK(rejects([&] { r.duty(1000); }));
        CHECK(rejects([&] { r.stop(1); }));
        CHECK(rejects([&] { r.inject_timeout(TimeoutInjection::target); }));
        r.heartbeat();
        r.status();
        r.tick();
        CHECK(io.writes.size() == writes && r.recovery_held());
        r.complete_recovery(first); // Trusted coordinator event, not a client command.
        CHECK(!r.recovery_held() && io.writes.size() == writes);
        CHECK(rejects([&] { r.complete_recovery(first); }));
        r.enable(1);
        r.velocity(1, {{1234, 0, 0, 0}}); // Buffered but not yet published.
        r.hold_for_recovery();
        CHECK(r.recovery_held() && io.source);
        r.prepare(config(), 1);
        CHECK(rejects([&] { r.complete_recovery(first); }));
        CHECK(r.recovery_held());
        r.complete_recovery(r.recovery_generation());
        r.enable(1);
        r.tick();
        CHECK(io.words[b::PUBLISHED_SEQUENCE] == 0); // Old buffered target discarded.
        r.velocity(1, {{5678, 0, 0, 0}});
        r.tick();
        CHECK(io.words[b::PUBLISHED_SEQUENCE] == 1);
        r.halt();
        r.prepare(config(), 1);
        r.disable(1); // Inhibiting commands remain available during hold.
        CHECK(r.recovery_held());
        io.time += p.client_lease_us;
        CHECK(rejects([&] { r.tick(); }));
        CHECK(io.source && r.recovery_held());
    }
    for (unsigned fault = 0; fault < 7; ++fault) {
        Fake io;
        Runtime r(io);
        r.hold_for_recovery(); // Dynamically opt in; subsequent prepare cannot bypass.
        r.prepare(config(), 1);
        auto generation = r.recovery_generation();
        if (fault == 0)
            ++io.words[reg::SESSION];
        if (fault == 1)
            ++io.words[reg::CLEANUP_REQUEST];
        if (fault == 2)
            io.words[reg::MONITOR_STATUS] = 7;
        if (fault == 3)
            ++io.words[reg::CONFIG_BASE];
        if (fault == 4)
            io.words[reg::STATUS] |= 1;
        if (fault == 5)
            io.fail = true;
        if (fault == 6)
            io.time += 500000;
        CHECK(rejects([&] { r.complete_recovery(generation); }));
        CHECK(r.recovery_held() && io.source);
        CHECK(rejects([&] { r.enable(1); }));
    }
    {
        Fake io;
        RuntimePolicy p;
        p.recovery_required = true;
        Runtime r(io, p);
        r.prepare(config(), 1);
        io.words[reg::STATUS] |= 1;
        CHECK(rejects([&] { r.tick(); }));
        CHECK(io.source && r.recovery_held());
    }
    for (unsigned change = 0; change < 9; ++change) {
        Fake io;
        RuntimePolicy policy;
        policy.recovery_required = true;
        Runtime r(io, policy);
        r.prepare(config(), 1);
        auto generation = r.recovery_generation();
        auto proof = r.attest_cleanup(generation);
        CHECK(proof.session == r.session() && proof.request == io.words[reg::CLEANUP_REQUEST]);
        CHECK(proof.configuration == generation && proof.disabled && proof.drained &&
              proof.causes_clear);
        bool injected = false;
        io.after_read = [&](uint32_t address) {
            if (injected || address != reg::CONFIG_BASE)
                return;
            injected = true; // Change after returning the old first configuration word.
            if (change == 0)
                ++io.words[reg::CONFIG_BASE];
            if (change == 1)
                ++io.words[reg::SESSION];
            if (change == 2)
                ++io.words[reg::CLEANUP_REQUEST];
            if (change == 3)
                io.words[reg::CLEANUP_STATUS] = 4;
            if (change == 4)
                io.words[reg::MONITOR_STATUS] = 7;
            if (change == 5)
                io.words[reg::STATUS] |= 1;
            if (change == 6)
                io.words[reg::CORE_RESET] = 1;
            if (change == 7)
                io.time += policy.transition_us;
            if (change == 8)
                io.fail = true;
        };
        CHECK(rejects([&] { r.attest_cleanup(generation); }));
        CHECK(injected && r.recovery_held() && io.source);
        CHECK(rejects([&] { r.enable(1); }));
    }
    for (unsigned failure = 0; failure < 7; ++failure) {
        Fake io;
        RuntimePolicy policy;
        policy.transition_us = 1000;
        Runtime runtime(io, policy);
        io.cleanup_failure = failure;
        if (failure == 0)
            io.words[reg::CLEANUP_CAPABILITY] = 0;
        if (failure == 6)
            io.words[reg::CLEANUP_REQUEST] = UINT32_MAX;
        CHECK(rejects([&] { runtime.prepare(config(), 1); }));
        CHECK(io.source && !(io.words[reg::STATUS] & 15));
        CHECK(rejects([&] { runtime.enable(1); }));
    }
    {
        Fake io;
        RuntimePolicy p;
        p.monitor_wait_us = 1000;
        Runtime r(io, p);
        io.words[reg::MONITOR_CAPABILITY] = 0;
        CHECK(rejects([&] { r.prepare(config(), 1); }));
        CHECK(io.source);
        io.words[reg::MONITOR_CAPABILITY] = 0x48423131;
        io.words[reg::MONITOR_STATUS] = 2;
        CHECK(rejects([&] { r.prepare(config(), 1); }));
        CHECK(io.source && io.time >= 1000);
        for (auto w : io.writes)
            CHECK(w.first != reg::MONITOR_ACK);
        io.words[reg::MONITOR_STATUS] = 7;
        r.prepare(config(), 1);
        CHECK(!(io.words[reg::MONITOR_STATUS] & 2U));
        CHECK(!(io.words[reg::STATUS] & 15)); // recovery never enables
    }
    {
        Fake io;
        Runtime r(io);
        CHECK(io.source);
        CHECK(rejects([&] { r.enable(1); }));
        r.prepare(config(), 15);
        CHECK(!io.source && r.session() == 1);
        r.enable(3);
        r.tick();
        CHECK(io.words[reg::REFRESH_BASE] == 1 && io.words[reg::REFRESH_BASE + 4] == 1);
        r.velocity(3, {{400000, -400000, 0, 0}});
        r.tick();
        CHECK(io.words[b::STAGE_MASK] == 3 && r.applied_sequence() == 1);
        CHECK(rejects([&] { r.velocity(1, {{400001, 0, 0, 0}}); }));
        // Different pending masks are never overwritten, even when disjoint.
        io.words[b::PENDING_MASK] = 1;
        r.velocity(2, {{0, 1000, 0, 0}});
        r.tick();
        CHECK(io.words[b::PUBLISHED_SEQUENCE] == 1);
        io.words[b::PENDING_MASK] = 0;
        io.time += 10000;
        r.tick();
        CHECK(io.words[b::PUBLISHED_SEQUENCE] == 2);
        r.stop(1);
        CHECK(io.words[reg::STOP] == 1);
        CHECK(rejects([&] { r.velocity(1, {{1000, 0, 0, 0}}); }));
        // STOP remains authorized/refreshed and must not fail readiness later.
        for (int i = 0; i < 30; ++i) {
            io.time += 20000;
            r.heartbeat();
            r.tick();
        }
        CHECK(!io.source);
        r.disable(2);
        CHECK(rejects([&] { r.enable(2); }));
        r.halt();
        CHECK(io.source);
        r.prepare(config(), 15);
        CHECK(r.session() == 2 && !(io.words[reg::STATUS] & 15));
    }
    {
        Fake io;
        Runtime r(io);
        r.prepare(config(), 1);
        r.enable(1);
        r.tick();
        io.time += 500000;
        CHECK(rejects([&] { r.tick(); }));
        CHECK(io.source);
        r.heartbeat();
        r.tick();
        CHECK(io.source); // no automatic recovery
    }
    {
        Fake io;
        Runtime r(io);
        r.prepare(config(), 1);
        r.enable(1);
        io.words[reg::STATUS] |= 1U << 16;
        CHECK(rejects([&] { r.tick(); }));
        CHECK(io.source);
    }
    {
        Fake io;
        Runtime r(io);
        r.prepare(config(), 1);
        r.enable(1);
        io.fail = true;
        CHECK(rejects([&] { r.tick(); }));
        CHECK(io.source);
    }
    {
        Fake io;
        io.hold_ready = true;
        Runtime r(io);
        r.prepare(config(), 1);
        r.enable(1);
        for (int i = 0; i < 24; ++i) {
            io.time += 20000;
            r.heartbeat();
            r.tick();
        }
        io.time += 20000;
        r.heartbeat();
        CHECK(rejects([&] { r.tick(); }));
        CHECK(io.source);
    }
    {
        Fake io;
        io.words[reg::SESSION] = UINT32_MAX;
        Runtime r(io);
        CHECK(rejects([&] { r.prepare(config(), 1); }));
        CHECK(io.source);
    }
    {
        Fake io;
        Runtime r(io);
        r.prepare(config(), 1);
        r.enable(1);
        io.halt_failure = true;
        CHECK(rejects([&] { r.halt(); }));
        CHECK(rejects([&] { r.enable(1); })); // Never report/reuse unconfirmed inhibition.
        io.halt_failure = false;
        r.halt();
        CHECK(io.source);
    }
    {
        Fake io;
        Runtime r(io);
        r.prepare(config(), 1);
        r.enable(1);
        r.velocity(1, {{1000, 0, 0, 0}});
        r.tick();
        CHECK(rejects([&] { r.inject_timeout(TimeoutInjection::target); }));
    }
    for (auto injection : {TimeoutInjection::target, TimeoutInjection::watchdog}) {
        Fake io;
        RuntimePolicy policy;
        policy.commissioning = true;
        Runtime r(io, policy);
        CHECK(rejects([&] { r.inject_timeout(injection); }));
        r.prepare(config(), 1);
        r.enable(1);
        r.velocity(1, {{1000, 0, 0, 0}});
        r.tick();
        const auto refresh = io.words[reg::REFRESH_BASE];
        const auto published = io.words[b::PUBLISHED_SEQUENCE];
        r.inject_timeout(injection);
        CHECK(rejects([&] { r.inject_timeout(injection); }));
        for (int i = 0; i < 5; ++i) {
            io.time += 20000;
            r.heartbeat();
            r.tick();
        }
        CHECK((io.words[reg::REFRESH_BASE] == refresh) ==
              (injection == TimeoutInjection::watchdog));
        CHECK((io.words[b::PUBLISHED_SEQUENCE] == published) ==
              (injection == TimeoutInjection::target));
        io.time += 20000;
        CHECK(rejects([&] { r.tick(); }));
        CHECK(io.source);
        // A fake without FPGA timers must hit the host fallback; never auto-resume.
        r.tick();
        CHECK(io.source);
        r.prepare(config(), 1);
        r.enable(1);
        r.velocity(1, {{1000, 0, 0, 0}});
        r.tick();
        CHECK(!io.source);
        r.inject_timeout(injection);
        io.words[reg::STATUS] |= 1U << 16;
        CHECK(rejects([&] { r.tick(); }));
        CHECK(io.source);
    }
    {
        Fake io;
        RuntimePolicy p;
        p.commissioning = true;
        p.direct_mask = 1;
        p.duty_limit_q16 = 32768;
        p.duty_floor_q16 = 6554;
        Runtime r(io, p);
        r.prepare(config(), 1);
        r.enable(1);
        r.tick();
        CHECK(rejects([&] { r.velocity(1, {{1000, 0, 0, 0}}); }));
        CHECK(rejects([&] { r.duty(6553); }));
        CHECK(rejects([&] { r.duty(32769); }));
        r.duty(32768);
        r.tick();
        CHECK(io.words[reg::DIRECT_DUTY] == 32768 && r.applied_sequence() == 1);
        r.duty(-6554);
        r.tick();
        CHECK(io.words[reg::DIRECT_DUTY] == static_cast<uint32_t>(-6554));
        r.duty(0);
        r.tick();
        CHECK(io.words[reg::DIRECT_DUTY] == 0);
        r.stop(1);
        r.tick();
        CHECK(rejects([&] { r.duty(32768); }));
        CHECK(rejects([&] { r.enable(1); }));
        r.prepare(config(), 1);
        r.enable(1);
        r.duty(16384);
        r.tick();
        r.inject_timeout(TimeoutInjection::target);
        const auto seq = r.applied_sequence();
        io.time += 20000;
        r.heartbeat();
        r.tick();
        CHECK(r.applied_sequence() == seq);
    }
    {
        Fake io;
        RuntimePolicy p;
        p.commissioning = true;
        p.direct_mask = 1;
        p.direct_pulse_us = 100000;
        p.duty_limit_q16 = 32768;
        Runtime r(io, p);
        r.prepare(config(), 1);
        r.enable(1);
        r.duty(16384);
        r.tick();
        CHECK(rejects([&] { r.inject_timeout(TimeoutInjection::target); }));
        io.time += 50000;
        r.heartbeat();
        r.duty(-16384);
        r.tick();
        CHECK(!io.source && (io.words[reg::STATUS] & 1));
        io.time += 50000;
        r.heartbeat();
        r.tick();
        CHECK(!(io.words[reg::STATUS] & 1) && io.words[reg::STOP] == 1);
        CHECK(rejects([&] { r.duty(16384); }));
        CHECK(rejects([&] { r.enable(1); }));
        r.prepare(config(), 1);
        r.enable(1);
        r.duty(16384);
        r.tick();
        r.duty(0);
        CHECK(!(io.words[reg::STATUS] & 1));
        CHECK(rejects([&] { r.duty(16384); }));
        r.prepare(config(), 1);
        r.enable(1);
        r.duty(16384);
        r.tick();
        // A command arriving after deadline cannot restart or extend a pulse,
        // even before the next periodic tick.
        io.time += 100000;
        CHECK(rejects([&] { r.duty(16384); }));
        CHECK(!(io.words[reg::STATUS] & 1));
        r.prepare(config(), 1);
        r.enable(1);
        r.duty(16384);
        r.tick();
        r.halt();
        r.prepare(config(), 1);
        r.enable(1);
        r.tick();
        CHECK(io.words[reg::DIRECT_COMMIT] == r.applied_sequence() || r.applied_sequence() == 0);
        CHECK(!io.source); // Rearm alone must not start another pulse.
    }
    std::cout
        << "PASS: lifecycle, 400RPM limits, atomic masks, STOP/rearm, watchdog refresh, lease/fault/access loss and readiness deadlines\n";
}
