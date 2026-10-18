#pragma once
#include "dmotor_sw/device.hpp"
#include <array>
#include <cstdint>
namespace dmotor_sw {
constexpr unsigned controller_motors = 4;
constexpr unsigned configuration_words = 20;
struct MotorConfiguration {
    uint32_t generation = 0;
    int32_t maximum_mrpm = 0;
    uint32_t acceleration_mrpm_s = 0, deceleration_mrpm_s = 0;
    uint32_t counts_numerator = 0, counts_denominator = 0;
    uint32_t position_tolerance_counts = 0, velocity_tolerance_mrpm = 0, settle_ticks = 0;
    int32_t kp_q16 = 0, ki_q16 = 0, duty_limit_q16 = 0, integral_limit_q16 = 0;
    uint32_t stop_timeout_us = 0, zero_timeout_cycles = 0, acquisition_timeout_cycles = 0;
    int32_t acquisition_duty_limit_q16 = 0;
};
// 400 RPM capability with per-motor limits. HLS remains the authoritative validator.
bool valid_configuration(const MotorConfiguration& config);
std::array<uint32_t, configuration_words> encode_configuration(const MotorConfiguration& config);
struct ControllerStatus {
    uint32_t raw = 0;
    uint8_t enabled = 0, configured = 0, ready = 0, stopped = 0, target_age_faults = 0;
    bool fault = false, shared_halt = false, blocked = false;
};
struct MotorTelemetry {
    int32_t requested_mrpm = 0, duty_q16 = 0, ramp_mrpm = 0, measured_mrpm = 0;
    bool pending = false, velocity_valid = false, velocity_timed_out = false;
    bool acquiring = false, warming_up = false, stopping = false;
    uint32_t engine_applied_sequence = 0, engine_pending_sequence = 0;
    uint32_t engine_published_sequence = 0, engine_session = 0;
    uint32_t flags = 0, engine_faults = 0, engine_rejections = 0, protection_faults = 0;
    uint64_t velocity_age_cycles = 0, sample_tick = 0, first_fault_cycles = 0;
    int64_t position_counts = 0;
};
struct TelemetrySnapshot {
    uint32_t capture_sequence = 0;
    std::array<MotorTelemetry, controller_motors> motors{};
};
class Protocol {
public:
    explicit Protocol(Device& device) : device_(device) {}
    // DMI2-only check works even when mailbox accesses are blocked by reset/HALT.
    bool compatible_controller();
    // Call only when the mailbox is accessible; no automatic reset or recovery.
    bool compatible_mailbox();
    ControllerStatus status();
    // Caller must hold core reset. No implicit reset, enable or acknowledgment.
    bool configure(unsigned motor, const MotorConfiguration& configuration);
    // One bounded attempt; false on concurrent capture, output left unchanged.
    // Throws IoError on transport failure, also leaving output unchanged.
    bool capture(TelemetrySnapshot& output);
private:
    Device& device_;
};
} // namespace dmotor_sw
