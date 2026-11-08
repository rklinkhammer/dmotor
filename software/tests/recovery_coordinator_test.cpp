#include "dmotor_sw/recovery_coordinator.hpp"
#include "dmotor/controller_registers.hpp"
#include "support/monitor_simulator.hpp"
#include <cstdlib>
#include <iostream>
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
using namespace dmotor_sw::monitor;
Frame wire(const Frame& f) {
    std::array<uint8_t, max_encoded> bytes{};
    auto size = encode(f, bytes);
    CHECK(size);
    Decoder decoder;
    Frame out;
    Decode result = Decode::incomplete;
    for (std::size_t i = 0; i < size; ++i)
        result = decoder.push(bytes[i], out);
    CHECK(result == Decode::frame);
    return out;
}
struct Joint {
    Fake io;
    Runtime runtime{io};
    uint64_t next_lifetime = 10;
    recovery::Coordinator coordinator{runtime, [&] {
                                          CHECK(io.source);
                                          return ++next_lifetime;
                                      }};
    Simulator monitor;
    uint64_t base = 0;
    void start(uint64_t owner = 1, uint64_t ps_id = 11, uint64_t mon_id = 22) {
        coordinator.prepare(owner, config(), 1);
        base = io.time;
        auto ph = coordinator.discover(), mh = monitor.boot(mon_id);
        CHECK(ph.source == ps_id);
        monitor.sample(base, 0, false);
        auto ma = monitor.receive(wire(ph), base);
        auto pa = coordinator.receive(wire(mh), base);
        coordinator.receive(wire(ma), base);
        monitor.receive(wire(pa.response), base);
        CHECK(coordinator.begin());
    }
    Frame exchange() {
        auto request = coordinator.poll(io.time);
        auto response = monitor.receive(wire(request), io.time);
        coordinator.receive(wire(response), io.time);
        return response;
    }
    void ready_to_finish() {
        exchange(); // request -> staged state
        exchange(); // commit -> ACK
        exchange(); // query -> SAFE; wait for physical ARM
        for (unsigned n = 1; n <= 17; ++n) {
            io.time = base + uint64_t(n) * 16000;
            monitor.sample(io.time, 0, n >= 2 && n <= 4);
            coordinator.tick();
        }
        CHECK(monitor.permits() && !coordinator.admitted());
        CHECK(coordinator.query_arm(io.time));
        exchange();
    }
    void recover() {
        ready_to_finish();
        CHECK(coordinator.finish());
        CHECK(coordinator.admitted());
    }
};
int main() {
    {
        Joint j;
        j.start();
        auto during = j.coordinator.ticket();
        CHECK(!j.coordinator.enable(during, 1));
        CHECK(rejects([&] { j.coordinator.prepare(1, config(), 1); }));
        j.recover();
        CHECK(!j.coordinator.enable(during, 1)); // Never restamp a held ingress item.
        auto old = j.coordinator.ticket();
        CHECK(j.coordinator.enable(old, 1));
        CHECK(j.coordinator.velocity(old, 1, {{1234, 0, 0, 0}})); // Buffered, not published.
        j.coordinator.cancel(); // Ownership loss revokes queue authority and targets.
        CHECK(j.io.source && !j.coordinator.admitted());
        CHECK(!j.coordinator.velocity(old, 1, {{1234, 0, 0, 0}}));
        j.start(1, 12, 23);
        j.recover(); // Even reused owner identity cannot reuse old ticket.
        auto writes = j.io.writes.size();
        CHECK(!j.coordinator.enable(old, 1));
        CHECK(!j.coordinator.velocity(old, 1, {{1234, 0, 0, 0}}));
        CHECK(!j.coordinator.stop(old, 1));
        CHECK(j.io.writes.size() == writes);
        auto fresh = j.coordinator.ticket();
        auto wrong = fresh;
        wrong.owner = 99;
        CHECK(!j.coordinator.enable(wrong, 1));
        CHECK(j.coordinator.enable(fresh, 1));
        j.coordinator.tick();
        CHECK(j.io.words[b::PUBLISHED_SEQUENCE] == 0); // No old target replay.
        CHECK(j.coordinator.velocity(fresh, 1, {{5678, 0, 0, 0}}));
        j.coordinator.tick();
        CHECK(j.io.words[b::PUBLISHED_SEQUENCE] == 1);
        j.io.time += 500000;
        CHECK(rejects([&] { j.coordinator.tick(); }));
        CHECK(j.io.source && !j.coordinator.enable(fresh, 1));
    }
    for (unsigned phase = 0; phase < 4; ++phase) {
        Joint j;
        j.start();
        if (phase > 0)
            j.exchange();
        if (phase > 1)
            j.exchange();
        if (phase > 2)
            j.exchange();
        j.coordinator.cancel();
        CHECK(j.io.source && !j.coordinator.finish());
        CHECK(!valid(j.coordinator.poll(j.io.time)));
        CHECK(!j.coordinator.enable(j.coordinator.ticket(), 1));
    }
    {
        Joint j;
        j.start();
        j.ready_to_finish();
        ++j.io.words[reg::CONFIG_BASE];
        CHECK(rejects([&] { j.coordinator.finish(); }));
        CHECK(j.io.source && !j.coordinator.admitted());
    }
    {
        Joint j;
        j.start();
        j.ready_to_finish();
        j.coordinator.cancel(); // Late ARMED/local-check outcome cannot finish.
        CHECK(!j.coordinator.finish());
        CHECK(!j.coordinator.admitted() && j.io.source);
    }
    {
        Joint j;
        j.start();
        j.recover();
        auto old = j.coordinator.ticket();
        auto hello = j.monitor.boot(23);
        j.coordinator.receive(wire(hello), j.io.time);
        CHECK(j.io.source && !j.coordinator.enable(old, 1));
    }
    {
        Joint j;
        j.start();
        j.recover();
        auto old = j.coordinator.ticket();
        j.io.halt_failure = true;
        CHECK(rejects([&] { j.coordinator.cancel(); }));
        CHECK(!j.coordinator.admitted() && !j.coordinator.enable(old, 1));
        j.io.halt_failure = false;
    }
    {
        Joint j;
        j.start();
        j.recover();
        auto old = j.coordinator.ticket();
        j.runtime.halt(); // External runtime lifecycle change invalidates coordinator.
        CHECK(!j.coordinator.enable(old, 1));
        j.coordinator.tick();
        CHECK(!j.coordinator.admitted() && j.io.source);
    }
    {
        Joint j;
        j.start();
        j.coordinator.poll(j.io.time);
        j.io.time += 100000;
        j.coordinator.poll(j.io.time);
        j.io.time += 100000;
        j.coordinator.poll(j.io.time);
        j.io.time += 100000;
        CHECK(!valid(j.coordinator.poll(j.io.time)));
        CHECK(j.io.source && !j.coordinator.admitted());
    }
    {
        Joint j;
        j.start();
        j.recover();
        auto ticket = j.coordinator.ticket();
        j.io.fail = true;
        CHECK(rejects([&] { j.coordinator.enable(ticket, 1); }));
        CHECK(j.io.source && !j.coordinator.admitted());
        CHECK(!j.coordinator.enable(ticket, 1));
    }
    {
        Joint j;
        j.start();
        ++j.io.words[reg::CLEANUP_REQUEST]; // Fault/identity change before first TX.
        CHECK(rejects([&] { j.coordinator.poll(j.io.time); }));
        CHECK(j.io.source && !valid(j.coordinator.poll(j.io.time)));
    }
    {
        Joint j;
        j.start();
        j.ready_to_finish();
        bool changed = false;
        j.io.after_read = [&](uint32_t address) {
            if (!changed && address == reg::CONFIG_BASE) {
                changed = true;
                ++j.io.words[reg::CLEANUP_SESSION];
            }
        };
        CHECK(rejects([&] { j.coordinator.finish(); }));
        CHECK(changed && j.io.source && !j.coordinator.admitted());
    }
    {
        Joint j;
        CHECK(j.coordinator.status().state == RecoveryState::held);
        j.start();
        CHECK(!j.coordinator.status().admission_open);
        j.ready_to_finish();
        CHECK(j.coordinator.status().state == RecoveryState::verifying);
        CHECK(j.coordinator.finish());
        CHECK(j.coordinator.status().state == RecoveryState::admitted);
        CHECK(j.coordinator.status().admission_open);
        j.coordinator.cancel();
        CHECK(j.coordinator.status().state == RecoveryState::held);
        CHECK(!j.coordinator.status().admission_open);
    }
    std::cout
        << "coordinator PASS: joint recovery, local proof, ownership loss, stale ingress tickets, lease, restart and HALT failure\n";
}
