#include "dmotor_sw/protocol.hpp"
#include "dmotor/capabilities.hpp"
#include "dmotor/controller_host.hpp"
#include "dmotor/timing.hpp"
#include <limits>
namespace dmotor_sw {
namespace {
struct Io {
    Device& device;
    uint32_t read(uint32_t offset) { return device.read32(offset); }
    bool write(uint32_t offset, uint32_t value) { device.write32(offset, value); return true; }
    void fence() { device.fence(); }
};
uint64_t wide(uint32_t low, uint32_t high) { return (uint64_t{high} << 32) | low; }
// Defined two's-complement decoding without implementation-defined unsigned casts.
int32_t signed32(uint32_t value) {
    if (value <= uint32_t{INT32_MAX}) return static_cast<int32_t>(value);
    return -1 - static_cast<int32_t>(UINT32_MAX - value);
}
int64_t signed64(uint64_t value) {
    if (value <= uint64_t{INT64_MAX}) return static_cast<int64_t>(value);
    return -1 - static_cast<int64_t>(UINT64_MAX - value);
}
}
bool valid_configuration(const MotorConfiguration& c) {
    using namespace dmotor::timing;
    if (!c.generation || c.maximum_mrpm < 1000 || c.maximum_mrpm > dmotor::capability::maximum_speed_mrpm ||
        !c.acceleration_mrpm_s || c.acceleration_mrpm_s > 10000000 ||
        !c.deceleration_mrpm_s || c.deceleration_mrpm_s > 10000000 ||
        !c.counts_numerator || c.counts_numerator > 1000000 ||
        !c.counts_denominator || c.counts_denominator > 1000000 ||
        c.velocity_tolerance_mrpm > static_cast<uint32_t>(c.maximum_mrpm) ||
        !c.settle_ticks || c.settle_ticks > settle_max_ticks ||
        c.kp_q16 < 0 || c.kp_q16 > 1048576 || c.ki_q16 < 0 || c.ki_q16 > 1048576 ||
        c.duty_limit_q16 < 0 || c.duty_limit_q16 > 65536 ||
        c.integral_limit_q16 < 0 || c.integral_limit_q16 > 65536 || !c.stop_timeout_us ||
        c.zero_timeout_cycles < zero_timeout_min_cycles || c.zero_timeout_cycles > zero_timeout_max_cycles ||
        c.acquisition_timeout_cycles < acquisition_timeout_min_cycles ||
        c.acquisition_timeout_cycles >= c.zero_timeout_cycles ||
        c.acquisition_duty_limit_q16 < 0 || c.acquisition_duty_limit_q16 > c.duty_limit_q16) return false;
    return uint64_t{static_cast<uint32_t>(c.maximum_mrpm)} * c.counts_numerator <=
           6000000000ULL * c.counts_denominator;
}
std::array<uint32_t, configuration_words> encode_configuration(const MotorConfiguration& c) {
    if (!valid_configuration(c)) throw std::invalid_argument("Invalid DMOTOR configuration");
    return {{c.generation, static_cast<uint32_t>(c.maximum_mrpm), c.acceleration_mrpm_s,
        c.deceleration_mrpm_s, c.counts_numerator, c.counts_denominator, c.position_tolerance_counts,
        c.velocity_tolerance_mrpm, c.settle_ticks, static_cast<uint32_t>(c.kp_q16),
        static_cast<uint32_t>(c.ki_q16), static_cast<uint32_t>(c.duty_limit_q16),
        static_cast<uint32_t>(c.integral_limit_q16), c.stop_timeout_us, c.zero_timeout_cycles,
        c.acquisition_timeout_cycles, static_cast<uint32_t>(c.acquisition_duty_limit_q16), 0, 0, 0}};
}
bool Protocol::compatible_controller() {
    return device_.read32(dmotor::controller_reg::IDENTITY) == dmotor::controller_reg::PROFILE_ID;
}
bool Protocol::compatible_mailbox() {
    using namespace dmotor::batch_reg;
    return device_.read32(IDENTITY) == PROFILE_ID && device_.read32(SCHEMA) == SCHEMA_VERSION &&
           device_.read32(MOTOR_COUNT) == controller_motors;
}
ControllerStatus Protocol::status() {
    if (!compatible_controller()) throw IoError("Unsupported controller identity");
    ControllerStatus s;
    s.raw = device_.read32(dmotor::controller_reg::STATUS);
    s.enabled = static_cast<uint8_t>(s.raw & 15U);
    s.configured = static_cast<uint8_t>((s.raw >> 4) & 15U);
    s.ready = static_cast<uint8_t>((s.raw >> 8) & 15U);
    s.stopped = static_cast<uint8_t>((s.raw >> 12) & 15U);
    s.fault = (s.raw & (1U << 16)) != 0;
    s.shared_halt = (s.raw & (1U << 17)) != 0;
    s.blocked = (s.raw & (1U << 18)) != 0;
    s.target_age_faults = static_cast<uint8_t>((s.raw >> 19) & 15U);
    return s;
}
bool Protocol::configure(unsigned motor, const MotorConfiguration& configuration) {
    if (motor >= controller_motors || !valid_configuration(configuration)) return false;
    if (status().enabled) return false;
    const auto encoded = encode_configuration(configuration);
    uint32_t words[configuration_words];
    for (unsigned i = 0; i < configuration_words; ++i) words[i] = encoded[i];
    Io io{device_};
    return dmotor::configure_motor(io, motor, words);
}
bool Protocol::capture(TelemetrySnapshot& output) {
    if (!compatible_controller()) throw IoError("Unsupported controller identity");
    Io io{device_};
    dmotor::MotorTelemetrySnapshot raw;
    if (!dmotor::capture_motor_telemetry(io, raw)) return false;
    TelemetrySnapshot result;
    result.capture_sequence = raw.sequence;
    for (unsigned m = 0; m < controller_motors; ++m) {
        const auto& w = raw.words[m]; auto& t = result.motors[m];
        t.requested_mrpm = signed32(w[0]); t.pending = w[1] != 0;
        t.engine_applied_sequence = w[2]; t.engine_pending_sequence = w[3];
        t.duty_q16 = signed32(w[4]); t.ramp_mrpm = signed32(w[5]); t.measured_mrpm = signed32(w[6]);
        t.flags = w[7]; t.velocity_valid = (w[7] & 1U) != 0; t.velocity_timed_out = (w[7] & 2U) != 0;
        t.acquiring = (w[7] & 4U) != 0; t.warming_up = (w[7] & 8U) != 0; t.stopping = (w[7] & 16U) != 0;
        t.engine_faults = w[8]; t.engine_rejections = w[9]; t.velocity_age_cycles = wide(w[10], w[11]);
        t.engine_published_sequence = w[12]; t.engine_session = w[13]; t.sample_tick = wide(w[14], w[15]);
        t.position_counts = signed64(wide(w[16], w[17])); t.protection_faults = w[18];
        t.first_fault_cycles = wide(w[19], w[20]);
    }
    output = result;
    return true;
}
} // namespace dmotor_sw
