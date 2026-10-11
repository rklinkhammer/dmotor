#include "dmotor/command_engine.hpp"
#include "dmotor/protocol.hpp"
extern "C" void dmotor_command_engine(bool reset, const uint32_t request[4], uint64_t now,
                                      bool executor_ready, int64_t position, int32_t max_speed_mrpm,
                                      bool upstream_drained, uint32_t generation,
                                      bool cancellation_valid, uint32_t cancellation_cause,
                                      bool completion_valid, uint32_t completion_epoch,
                                      uint32_t completion_sequence, uint32_t completion_detail,
                                      uint32_t reply[16]) {
    static dmotor::CommandRing ring;
    bool accepted = false;
    bool activated = false;
    uint32_t read_data = 0;
    if (reset) {
        ring = dmotor::CommandRing{};
    } else if (cancellation_valid) {
        // Hardware protection is independent and already inhibited the output.
        // This path only updates accounting; cancellation wins completion/publication.
        ring.cancel(now, cancellation_cause);
    } else {
        if (completion_valid && ring.busy() && ring.epoch() == completion_epoch &&
            ring.active()[2] == completion_sequence) {
            ring.finish(now, 0, completion_detail);
        }
        switch (static_cast<dmotor::EngineOperation>(request[0])) {
            case dmotor::EngineOperation::poll:
                accepted = true;
                break;
            case dmotor::EngineOperation::stage:
                accepted = ring.stage(request[1], request[2], request[3]);
                break;
            case dmotor::EngineOperation::publish:
                accepted = ring.publish(request[1], request[2]);
                break;
            case dmotor::EngineOperation::reopen:
                accepted = !executor_ready && ring.reopen(upstream_drained, generation);
                break;
            case dmotor::EngineOperation::quiesce:
                ring.quiesce();
                accepted = true;
                break;
            case dmotor::EngineOperation::acknowledge_completions:
                accepted = ring.acknowledge_completions(request[1]);
                break;
            case dmotor::EngineOperation::acknowledge_cancellation:
                ring.acknowledge_cancellation();
                accepted = true;
                break;
            case dmotor::EngineOperation::read_completion: {
                dmotor::Completion completion{};
                if (request[2] < 8 && ring.read_completion(request[1], completion)) {
                    read_data = completion[request[2]];
                    accepted = true;
                }
                break;
            }
            case dmotor::EngineOperation::read_active:
                if (ring.busy() && request[1] < 16) {
                    read_data = ring.active()[request[1]];
                    accepted = true;
                }
                break;
            default:
                break;
        }
        // Only poll activates work. The autonomous adapter must schedule polls
        // fairly even under continuous CPU traffic. No CPU-issued control ticks.
        if (request[0] == 0 && executor_ready && !ring.busy()) {
            ring.capture(now, position, max_speed_mrpm);
            activated = ring.busy();
        }
    }
    const auto& cancel = ring.cancellation();
    reply[0] = accepted;
    reply[1] =
        (ring.busy() ? 1U : 0U) | (ring.admission_open() ? 2U : 0U) | (cancel.valid ? 4U : 0U);
    reply[2] = ring.producer();
    reply[3] = ring.consumer();
    reply[4] = ring.completion_producer();
    reply[5] = ring.epoch();
    reply[6] = activated;
    reply[7] = read_data;
    reply[8] = cancel.epoch;
    reply[9] = cancel.active_valid;
    reply[10] = cancel.active_sequence;
    reply[11] = cancel.pending_begin;
    reply[12] = cancel.pending_end;
    reply[13] = cancel.cause;
    reply[14] = static_cast<uint32_t>(cancel.timestamp);
    reply[15] = static_cast<uint32_t>(cancel.timestamp >> 32);
}
