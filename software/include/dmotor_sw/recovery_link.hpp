#pragma once
#include "dmotor_sw/recovery_model.hpp"
#include "dmotor_sw/session_link.hpp"
#include <memory>
namespace dmotor_sw {
namespace recovery {
// Isolated PS wire adapter. Caller supplies verified cleanup and durable lifetimes.
class Link {
  public:
    monitor::Frame discover(uint64_t lifetime, bool inhibited);
    monitor::Reception receive(const monitor::Frame& frame, uint64_t now_us);
    bool begin(const Cleanup& cleanup, uint64_t now_us);
    monitor::Frame poll(uint64_t now_us);
    bool query_arm(uint64_t now_us);
    bool confirm_local(const Cleanup& cleanup, uint64_t now_us);
    void invalidate();
    bool admission_open() const;
    bool halt_required() const;
    Phase phase() const;

  private:
    monitor::SessionLink link_;
    std::unique_ptr<Model> model_;
    uint32_t logical_sequence_ = 0;
    monitor::Frame retry_{};
};
} // namespace recovery
} // namespace dmotor_sw
