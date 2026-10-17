#pragma once
#include "dmotor/capabilities.hpp"
#include "dmotor/batch_host.hpp"
#include <array>
#include <cstdint>

namespace dmotor_sw {
constexpr unsigned motor_count = dmotor::batch_reg::CHANNELS;
constexpr uint32_t all_motors = (1U << motor_count) - 1U;
constexpr uint32_t control_period_us = 1000;
constexpr uint32_t initial_target_timeout_ticks = 100;
constexpr int32_t maximum_velocity_mrpm = dmotor::capability::maximum_speed_mrpm;

// Semantic admission subset, NOT the hardware's complete 20-word configuration.
struct MotorLimits {
    uint32_t generation = 0;
    uint32_t maximum_mrpm = maximum_velocity_mrpm;
};
struct Timeouts {
    uint32_t authorization_ticks;
    uint32_t target_ticks;
    uint32_t maintenance_lease_ticks;
};
enum class Fault { none, authorization, target_age, ownership_lost, requested_halt };
struct Snapshot {
    uint64_t tick = 0;
    uint32_t enabled_mask = 0;
    uint32_t tracking_mask = 0;
    bool halted = true;
    Fault fault = Fault::none;
    dmotor::AppliedBatchSnapshot applied{};
    std::array<int32_t, motor_count> requested_mrpm{};
    // No fabricated measured speed/position: this model has no motor plant.
};

// Single-threaded deterministic service/device model. Not a Linux driver or
// independent protection implementation. advance() is driven by the test clock.
class Model {
public:
    explicit Model(Timeouts timeouts);
    uint64_t acquire(); // Exclusive maintenance lease; zero means refused.
    bool renew(uint64_t owner);
    bool release(uint64_t owner); // HALT ALL before relinquishing ownership.
    bool rearm(uint64_t owner, uint32_t session,
               const std::array<MotorLimits, motor_count>& limits);
    bool enable(uint64_t owner, uint32_t mask);
    bool publish(uint64_t owner, const dmotor::VelocityBatch& batch);
    // Refresh sampled at the next tick; expiration wins an equal-boundary refresh.
    bool refresh(uint64_t owner, uint32_t mask);
    bool halt(uint64_t owner);
    void advance(uint32_t ticks = 1);
    Snapshot snapshot() const;
private:
    bool owns(uint64_t owner) const;
    void trip(Fault fault);
    Timeouts timeouts_;
    dmotor::AtomicMailbox mailbox_{};
    dmotor::BatchAdmission admission_{};
    Snapshot state_{};
    uint64_t owner_ = 0, next_owner_ = 0, lease_tick_ = 0;
    uint32_t last_session_ = 0, refresh_mask_ = 0;
    std::array<uint64_t, motor_count> authorization_tick_{}, target_tick_{};
};

// Models orderly client close. An actual service must separately detect socket
// loss and lease expiry; destructors cannot handle SIGKILL or a dead service.
class MaintenanceSession {
public:
    explicit MaintenanceSession(Model& model) : model_(model), token_(model.acquire()) {}
    ~MaintenanceSession() { if (token_) model_.release(token_); }
    MaintenanceSession(const MaintenanceSession&) = delete;
    MaintenanceSession& operator=(const MaintenanceSession&) = delete;
    uint64_t token() const { return token_; }
private:
    Model& model_;
    uint64_t token_;
};
} // namespace dmotor_sw
