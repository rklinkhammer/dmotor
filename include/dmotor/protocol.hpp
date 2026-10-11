#pragma once
#include "dmotor/ring_config.hpp"
#include <array>
#include <cstdint>

namespace dmotor {
using Descriptor = std::array<uint32_t, 16>;
using Completion = std::array<uint32_t, 8>;
enum class Error : uint32_t {
    none,
    opcode,
    reserved,
    sequence,
    epoch,
    generation,
    range,
    overflow
};
struct Cancellation {
    bool valid = false;
    bool active_valid = false;
    uint32_t epoch = 0;
    uint32_t active_sequence = 0;
    uint32_t pending_begin = 0;
    uint32_t pending_end = 0;
    uint32_t cause = 0;
    uint64_t timestamp = 0;
};
int32_t signed_word(uint32_t word);
int64_t signed_pair(uint32_t low, uint32_t high);
Error validate(const Descriptor& descriptor, uint32_t sequence, uint32_t epoch, uint32_t generation,
               int64_t position, int32_t max_speed_mrpm);

// Synthesizable ring state used by the native reference and HLS engine.
// This class is not an AXI peripheral or a motion controller.
// All calls represent serialized, completed device transactions. The platform
// must prove upstream write draining before passing drained=true to reopen().
class CommandRing {
  public:
    bool reopen(bool drained, uint32_t generation, uint32_t initial_counter = 0);
    void quiesce();
    bool stage(uint32_t slot, uint32_t word, uint32_t value);
    bool publish(uint32_t requested_producer, uint32_t session);
    bool offer(Descriptor& descriptor) const;
    bool capture(uint64_t now, int64_t position, int32_t max_speed_mrpm);
    bool finish(uint64_t now, uint32_t outcome = 0, uint32_t detail = 0);
    void cancel(uint64_t now, uint32_t cause);
    bool acknowledge_completions(uint32_t requested_consumer);
    void acknowledge_cancellation();
    bool read_completion(uint32_t counter, Completion& result) const;
    const Descriptor& active() const;
    bool busy() const;
    bool admission_open() const;
    uint32_t producer() const;
    uint32_t consumer() const;
    uint32_t completion_producer() const;
    uint32_t epoch() const;
    const Cancellation& cancellation() const;

  private:
    std::array<Descriptor, ring_depth> slots_{};
    std::array<Completion, ring_depth> completions_{};
    Descriptor active_{};
    Cancellation cancellation_{};
    uint32_t producer_ = 0, consumer_ = 0;
    uint32_t completion_producer_ = 0, completion_consumer_ = 0;
    uint32_t epoch_ = 0, generation_ = 0;
    uint64_t activation_ = 0;
    bool admission_ = false, busy_ = false;
};
} // namespace dmotor
