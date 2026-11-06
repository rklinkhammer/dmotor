#pragma once
#include "dmotor_sw/recovery_link.hpp"
#include "dmotor_sw/runtime.hpp"
#include <functional>
namespace dmotor_sw {
namespace recovery {
// Internal serialized integration candidate. No socket commands or persistence.
class Coordinator {
  public:
    struct Ticket {
        uint64_t owner = 0, generation = 0;
    };
    Coordinator(Runtime& runtime, std::function<uint64_t()> reserve_session);
    ~Coordinator();
    void prepare(uint64_t owner, const std::array<MotorConfiguration, 4>& configs, uint32_t mask);
    monitor::Frame discover();
    bool begin();
    monitor::Reception receive(const monitor::Frame& frame, uint64_t now);
    monitor::Frame poll(uint64_t now);
    bool query_arm(uint64_t now);
    bool finish();
    void cancel(); // Ownership loss, HALT, transport restart or fault. May throw on HALT failure.
    void tick();
    Ticket ticket() const; // Stamp at ingress, never at later dispatch.
    bool enable(Ticket ticket, uint32_t mask);
    bool velocity(Ticket ticket, uint32_t mask, const std::array<int32_t, 4>& targets);
    bool stop(Ticket ticket, uint32_t mask);
    bool admitted() const;

  private:
    bool current(Ticket ticket) const;
    void check();
    Runtime& runtime_;
    std::function<uint64_t()> reserve_session_;
    uint64_t lifetime_ = 0, last_lifetime_ = 0;
    monitor::Frame hello_{};
    Link link_;
    uint64_t owner_ = 0, generation_ = 0;
    bool prepared_ = false, recovering_ = false;
};
} // namespace recovery
} // namespace dmotor_sw
