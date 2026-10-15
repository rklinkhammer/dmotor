#include "dmotor_sw/version.hpp"
#include "dmotor_sw/protocol.hpp"
#include "dmotor/controller_registers.hpp"
#include "dmotor/batch_registers.hpp"
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (false)
using namespace dmotor_sw;
namespace reg = dmotor::controller_reg;
struct Fake : Device {
    std::map<uint32_t, uint32_t> words;
    std::vector<uint32_t> writes;
    unsigned operations = 0, fail_at = 0, fences = 0, capture_reads = 0;
    bool torn = false;
    Fake() {
        words[reg::IDENTITY] = reg::PROFILE_ID;
        words[dmotor::batch_reg::IDENTITY] = dmotor::batch_reg::PROFILE_ID;
        words[dmotor::batch_reg::SCHEMA] = dmotor::batch_reg::SCHEMA_VERSION;
        words[dmotor::batch_reg::MOTOR_COUNT] = controller_motors;
    }
    void access() { if (++operations == fail_at) throw IoError("Injected I/O error"); }
    uint32_t read32(uint32_t offset) override {
        access(); CHECK(offset % 4 == 0);
        if (offset == reg::SNAPSHOT && ++capture_reads == 2 && torn) ++words[offset];
        return words[offset];
    }
    void write32(uint32_t offset, uint32_t value) override {
        access(); CHECK(offset % 4 == 0); writes.push_back(offset);
        if (offset == reg::SNAPSHOT) { CHECK(value == 1); ++words[offset]; }
        else words[offset] = value;
    }
    void fence() override { access(); ++fences; }
};
MotorConfiguration configuration() {
    return {7, 200000, 1000000, 2000000, 10296, 5, 1, 100, 2,
            655, 65536, 32768, 16384, 1000000, 2000000, 1000000, 8192};
}
int main(int argc, char** argv) {
    if (dmotor_sw::startup_version("dmotor-protocol-tests", argc, argv)) return 0;
    const uint32_t expected[20] = {7,200000,1000000,2000000,10296,5,1,100,2,
        655,65536,32768,16384,1000000,2000000,1000000,8192,0,0,0};
    {
        Fake io; Protocol protocol(io); CHECK(protocol.compatible_controller()); CHECK(protocol.compatible_mailbox());
        CHECK(protocol.configure(3, configuration())); CHECK(io.writes.size() == 21); CHECK(io.fences == 2);
        for (unsigned i = 0; i < 20; ++i) {
            CHECK(io.writes[i] == reg::CONFIG_BASE + 3 * reg::CONFIG_STRIDE + 4 * i);
            CHECK(io.words[io.writes[i]] == expected[i]);
        }
        CHECK(io.writes.back() == reg::CONFIG_BASE + 3 * reg::CONFIG_STRIDE + reg::CONFIG_COMMIT);
        CHECK(!protocol.configure(4, configuration()));
        io.words[reg::STATUS] = 1; const auto count = io.writes.size();
        CHECK(!protocol.configure(0, configuration())); CHECK(io.writes.size() == count);
        io.words[reg::STATUS] = 0x005f4321;
        auto s = protocol.status();
        CHECK(s.enabled == 1 && s.configured == 2 && s.ready == 3 && s.stopped == 4);
        CHECK(s.fault && s.shared_halt && s.blocked && s.target_age_faults == 11);
        io.words[dmotor::batch_reg::SCHEMA] = 99; CHECK(!protocol.compatible_mailbox());
        io.words[reg::IDENTITY] = 0; CHECK(!protocol.compatible_controller());
        bool caught = false; try { protocol.configure(0, configuration()); } catch (const IoError&) { caught = true; }
        CHECK(caught); CHECK(io.writes.size() == count);
    }
    {
        auto c = configuration(); c.maximum_mrpm = 200001; CHECK(!valid_configuration(c));
        c = configuration(); c.counts_denominator = 0; CHECK(!valid_configuration(c));
        c = configuration(); c.acquisition_timeout_cycles = c.zero_timeout_cycles; CHECK(!valid_configuration(c));
        c = configuration(); c.kp_q16 = -1; CHECK(!valid_configuration(c));
        c = configuration(); c.counts_numerator = 1000000; c.counts_denominator = 1;
        CHECK(!valid_configuration(c)); // Encoder event-rate limit.
        Fake io; Protocol protocol(io); CHECK(!protocol.configure(0, c)); CHECK(io.operations == 0);
    }
    {
        Fake io; Protocol protocol(io);
        for (unsigned motor = 0; motor < 4; ++motor) {
            const uint32_t values[21] = {0xffffffff,1,30,31,0x80000000,0xfffffc18,1000,
                31,0x80000020,4,0x12345678,2,32,7,9,3,0,0x80000000,0x80,11,4};
            for (unsigned w = 0; w < 21; ++w)
                io.words[reg::TELEMETRY_BASE + motor * reg::TELEMETRY_STRIDE + 4 * w] = values[w];
        }
        TelemetrySnapshot result; CHECK(protocol.capture(result)); CHECK(result.capture_sequence == 1);
        for (auto& t : result.motors) {
            CHECK(t.requested_mrpm == -1 && t.duty_q16 == INT32_MIN && t.ramp_mrpm == -1000);
            CHECK(t.measured_mrpm == 1000 && t.position_counts == INT64_MIN);
            CHECK(t.pending && t.velocity_valid && t.velocity_timed_out && t.acquiring && t.warming_up && t.stopping);
            CHECK(t.engine_applied_sequence == 30 && t.engine_pending_sequence == 31 && t.engine_published_sequence == 32);
            CHECK(t.engine_session == 7 && t.engine_faults == 0x80000020 && t.engine_rejections == 4);
            CHECK(t.velocity_age_cycles == 0x212345678ULL && t.sample_tick == 0x300000009ULL);
            CHECK(t.first_fault_cycles == 0x40000000bULL && t.protection_faults == 0x80);
        }
        io.capture_reads = 0; io.torn = true; CHECK(!protocol.capture(result));
        CHECK(result.capture_sequence == 1 && result.motors[0].position_counts == INT64_MIN);
    }
    // Fail each access, including fences. No failure may publish a partial snapshot.
    unsigned capture_operations = 0, configure_operations = 0;
    { Fake io; Protocol p(io); TelemetrySnapshot t; CHECK(p.capture(t)); capture_operations = io.operations; }
    { Fake io; Protocol p(io); CHECK(p.configure(0, configuration())); configure_operations = io.operations; }
    for (unsigned fail = 1; fail <= capture_operations; ++fail) {
        Fake io; io.fail_at = fail; Protocol p(io); TelemetrySnapshot result; result.capture_sequence = 99;
        bool caught = false; try { p.capture(result); } catch (const IoError&) { caught = true; }
        CHECK(caught && result.capture_sequence == 99);
    }
    for (unsigned fail = 1; fail <= configure_operations; ++fail) {
        Fake io; io.fail_at = fail; Protocol p(io); bool caught = false;
        try { p.configure(0, configuration()); } catch (const IoError&) { caught = true; }
        CHECK(caught); // Final fence failure can leave commit effective; never report success/retry blindly.
        if (fail < configure_operations - 1)
            for (auto offset : io.writes) CHECK(offset != reg::CONFIG_BASE + reg::CONFIG_COMMIT);
    }
    std::cout << "PASS: typed configuration, ABI checks, telemetry decoding/coherence and transport failure propagation\n";
}
