#pragma once
#include "dmotor_sw/protocol.hpp"
#include "dmotor/batch_host.hpp"
namespace dmotor_sw {
struct Platform : Device {
    virtual void halt(bool asserted) = 0;
    virtual uint64_t now_us() const = 0;
    virtual void sleep_us(uint32_t duration) = 0;
};
enum class TimeoutInjection { none, target, watchdog };
struct RuntimePolicy {
    bool commissioning = false;
    uint32_t target_period_us = 10000;
    uint32_t refresh_period_us = 20000;
    uint32_t authorization_us = 100000;
    uint32_t target_timeout_us = 100000;
    uint32_t client_lease_us = 500000;
    uint32_t readiness_us = 500000;
    uint32_t transition_us = 20000;
};
class Runtime {
public:
    Runtime(Platform& platform, RuntimePolicy policy = {});
    ~Runtime();
    void prepare(const std::array<MotorConfiguration,4>& configs, uint32_t mask);
    void enable(uint32_t mask);
    void velocity(uint32_t mask, const std::array<int32_t,4>& targets);
    void stop(uint32_t mask);
    void disable(uint32_t mask);
    void halt();
    void heartbeat();
    void inject_timeout(TimeoutInjection injection);
    void tick();
    ControllerStatus status();
    TelemetrySnapshot capture();
    uint32_t session() const { return session_; }
    uint32_t applied_sequence() const { return applied_; }
private:
    void wait_status(uint32_t mask, uint32_t expected);
    void write(uint32_t offset, uint32_t value);
    void refresh(uint32_t mask);
    Platform& io_;
    Protocol protocol_;
    RuntimePolicy policy_;
    std::array<MotorConfiguration,4> configs_{};
    std::array<int32_t,4> targets_{};
    std::array<uint32_t,4> refresh_sequence_{};
    uint32_t session_=0, sequence_=0, configured_=0, enabled_=0, tracking_=0, stopped_=0, applied_=0;
    uint64_t lease_=0, next_refresh_=0, next_target_=0, ready_deadline_=0;
    std::array<uint64_t,4> stop_deadline_{};
    TimeoutInjection injection_=TimeoutInjection::none;
    uint64_t injection_deadline_=0;
    bool prepared_=false, halted_=true;
};
}
