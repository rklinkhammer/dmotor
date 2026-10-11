#pragma once
#include <cstdint>
namespace dmotor {
// Internal transaction protocol, not the processor MMIO ABI. Adapter owns bus
// ordering and samples every request for the full HLS transaction.
enum class EngineOperation : uint32_t {
    poll = 0,
    stage = 1,
    publish = 2,
    reopen = 3,
    quiesce = 4,
    acknowledge_completions = 5,
    acknowledge_cancellation = 6,
    read_completion = 7,
    read_active = 8
};
} // namespace dmotor
// Request: [0] operation; [1..3] operands. Reply: [0] request accepted;
// [1] status (busy=1, admission=2, cancellation=4); [2..5] producer,
// consumer, completion producer, epoch; [6] command activated; [7] read data;
// [8..15] cancellation epoch, active-valid, sequence, pending begin/end,
// cause, timestamp low/high. Remaining words reserved zero.
// executor_ready is a trusted hardware input, not a software enable bit.
// completion_valid and cancellation_valid must be held until transaction done.
extern "C" void dmotor_command_engine(bool reset, const uint32_t request[4], uint64_t now,
                                      bool executor_ready, int64_t position, int32_t max_speed_mrpm,
                                      bool upstream_drained, uint32_t generation,
                                      bool cancellation_valid, uint32_t cancellation_cause,
                                      bool completion_valid, uint32_t completion_epoch,
                                      uint32_t completion_sequence, uint32_t completion_detail,
                                      uint32_t reply[16]);
