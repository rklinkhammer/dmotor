#pragma once
#include "dmotor_sw/monitor_wire.hpp"
#include <cstdint>
namespace dmotor_sw {
namespace recovery {
// Post-pairing semantic model only. No UART/MMIO, persistence or motion side effects.
struct Cleanup {
    uint32_t session = 0, request = 0;
    uint64_t configuration = 0; // Caller-owned identity for the complete configuration.
    bool disabled = false, drained = false, causes_clear = false;
};
struct Pair {
    uint64_t ps = 0, monitor = 0; // Already mutually confirmed, durably allocated IDs.
};
enum class Phase {
    held,
    request,
    commit,
    query,
    waiting_arm,
    local_check,
    complete,
    failed,
    unknown
};
enum class Operation {
    ping,
    status,
    capture,
    disable,
    halt,
    quit,
    prepare,
    enable,
    velocity,
    duty,
    stop,
    timeout_injection
};
enum class Message { none, recover_request, recover_commit, state_query };
struct Transmission {
    Message message = Message::none;
    Pair pair{};
    uint32_t transaction = 0, epoch = 0, sequence = 0;
    uint64_t challenge = 0;
    Cleanup cleanup{};
};
struct Reply {
    Pair pair{};
    uint32_t transaction = 0, epoch = 0;
    uint64_t challenge = 0;
    monitor::State state = monitor::State::boot;
    uint32_t causes = 0, completed_transaction = 0, completed_epoch = 0, pl_session = 0;
};
// Replies have passed codec, pairing, sequence and duplicate-content validation.
// Those transport checks are deliberately outside this post-pairing model.
class Model {
  public:
    explicit Model(Pair pair);
    bool begin(const Cleanup& cleanup, uint64_t now_us);
    bool recovered(const Reply& reply, uint64_t now_us);
    bool committed(const Reply& reply, uint64_t now_us);
    bool state_reply(const Reply& reply, uint64_t now_us);
    bool query_arm(uint64_t now_us);
    bool confirm_local(const Cleanup& cleanup, uint64_t now_us);
    Transmission poll(uint64_t now_us);
    void abort(); // Local fault, ownership loss, HALT, quit or detected peer restart.
    bool allowed(Operation operation) const;
    bool admission_open() const {
        return phase_ == Phase::complete;
    }
    bool halt_required() const {
        return halt_;
    }
    Phase phase() const {
        return phase_;
    }

  private:
    bool clock(uint64_t now);
    bool matches(const Reply& reply) const;
    bool completed(const Reply& reply) const;
    bool send(Message message, Phase phase, uint64_t now);
    bool query(uint64_t now);
    Pair pair_;
    Cleanup cleanup_{};
    Transmission transmission_{};
    Phase phase_ = Phase::held;
    uint64_t last_now_ = 0, sent_at_ = 0, challenge_ = 0;
    uint32_t transaction_ = 0, epoch_ = 0, sequence_ = 0;
    uint32_t last_request_ = 0, last_pl_session_ = 0;
    unsigned attempts_ = 0;
    bool queued_ = false, halt_ = true;
};
} // namespace recovery
} // namespace dmotor_sw
