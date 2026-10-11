#include "dmotor/dispatcher.hpp"
#include "dmotor/command_engine.hpp"
namespace dmotor {
void Dispatcher::service(bool reset, const uint32_t request[4], bool tick, bool authorized,
                         bool cancel, uint32_t cause, bool stop, bool capture_fault, bool drained,
                         uint64_t now, const EncoderSample& encoder, int64_t position,
                         const ControllerConfig& config, uint32_t reply[40]) {
    bool accepted = false, activated = false;
    uint32_t read_data = 0;
    if (reset) {
        *this = Dispatcher{};
    } else {
        // Live inhibition/cancellation wins bus mutation, activation and completion.
        if (capture_fault) {
            faults_ |= 2;
        }
        const bool lost_authorization = tick_authorized_ && !authorized;
        if (cancel || capture_fault || lost_authorization) {
            ring_.cancel(now, capture_fault ? 2U : cancel ? cause : 0x80000000U);
            cancel_pending_ = true;
            stop_pending_ = false;
        } else if (stop && !cancel_pending_ && !faults_) {
            ring_.cancel(now, 0x40000000U);
            stop_pending_ = true;
        }
        const bool protective = cancel || capture_fault || lost_authorization;
        if (!protective && !stop) {
            switch (static_cast<EngineOperation>(request[0])) {
                case EngineOperation::poll:
                    accepted = true;
                    break;
                case EngineOperation::stage:
                    accepted = ring_.stage(request[1], request[2], request[3]);
                    break;
                case EngineOperation::publish:
                    accepted = ring_.publish(request[1], request[2]);
                    break;
                case EngineOperation::reopen:
                    accepted = !authorized && !tick_authorized_ && !cancel_pending_ &&
                               !stop_pending_ && !faults_ && !last_.faults &&
                               config.motion.generation &&
                               ring_.reopen(drained, config.motion.generation);
                    break;
                case EngineOperation::quiesce:
                    ring_.quiesce();
                    accepted = true;
                    break;
                case EngineOperation::acknowledge_completions:
                    accepted = ring_.acknowledge_completions(request[1]);
                    break;
                case EngineOperation::acknowledge_cancellation:
                    ring_.acknowledge_cancellation();
                    accepted = true;
                    break;
                case EngineOperation::read_completion:
                    accepted = ring_.read_completion_word(request[1], request[2], read_data);
                    break;
                case EngineOperation::read_active:
                    if (ring_.busy() && request[1] < 16) {
                        read_data = ring_.active()[request[1]];
                        accepted = true;
                    }
                    break;
                default:
                    break;
            }
        }
        if (tick) {
            Descriptor offered{};
            const bool can_offer = authorized && !cancel_pending_ && !stop_pending_ && !faults_ &&
                                   ring_.offer(offered);
            Error error = can_offer ? validate(offered, ring_.consumer(), ring_.epoch(),
                                               config.motion.generation, position,
                                               config.motion.max_speed_mrpm)
                                    : Error::none;
            // Ring ownership and active configuration must agree before activation.
            if (can_offer && error == Error::none && offered[4] != ring_.generation_) {
                error = Error::generation;
            }
            auto next = controller_.step_impl(
                false, authorized && !cancel_pending_ && !faults_, capture_fault, now, encoder,
                position, ring_.epoch() ? ring_.epoch() : 1, can_offer && error == Error::none,
                offered, ack_, stop_pending_, config, true);
            ack_ = next.motion.completion_valid;
            tick_authorized_ = authorized && !cancel_pending_ && !faults_;
            if (next.faults) {
                ring_.cancel(now, 0x20000000U | next.faults);
                faults_ |= next.faults;
            } else if (!cancel_pending_ && !stop_pending_) {
                if (next.motion.accepted) {
                    if (!ring_.capture_checked(now, error) || !ring_.busy()) {
                        faults_ |= 0x80000000U;
                    } else {
                        activated = true;
                    }
                } else if (can_offer && error != Error::none && next.motion.ready) {
                    ring_.capture_checked(now, error); // rejected completion, zero activation
                }
                if (next.motion.completion_valid) {
                    if (ring_.busy() && ring_.active()[2] == next.motion.sequence &&
                        ring_.epoch() == next.motion.epoch) {
                        ring_.finish(next.motion.terminal, next.motion.outcome, next.motion.detail);
                    } else {
                        faults_ |= 0x80000000U;
                    }
                }
            }
            // Canceled controller completions are acknowledged locally, never retired twice.
            cancel_pending_ = false;
            stop_pending_ = false;
            if (faults_) {
                ring_.cancel(now, 0x20000000U | faults_);
                next.motion.duty_q16 = 0;
                activated = false;
            }
            last_ = next;
        }
    }
    const auto& c = ring_.cancellation();
    reply[0] = accepted;
    reply[1] = (ring_.busy() ? 1U : 0U) | (ring_.admission_open() ? 2U : 0U) | (c.valid ? 4U : 0U);
    reply[2] = ring_.producer();
    reply[3] = ring_.consumer();
    reply[4] = ring_.completion_producer();
    reply[5] = ring_.epoch();
    reply[6] = activated;
    reply[7] = read_data;
    reply[8] = c.epoch;
    reply[9] = c.active_valid;
    reply[10] = c.active_sequence;
    reply[11] = c.pending_begin;
    reply[12] = c.pending_end;
    reply[13] = c.cause;
    reply[14] = static_cast<uint32_t>(c.timestamp);
    reply[15] = static_cast<uint32_t>(c.timestamp >> 32);
    encode_controller_result(last_, reply + 16);
    reply[32] |= faults_;
    uint32_t motion_flags = reply[18] & ~8U;
    if (cancel_pending_ || !authorized || faults_) {
        reply[16] = 0;
        motion_flags &= ~1U;
    }
    // Report committed activation, not provisional controller acceptance.
    reply[18] = motion_flags | (activated ? 8U : 0U);
}
} // namespace dmotor
extern "C" void dmotor_dispatcher(bool reset, const uint32_t request[4], bool tick, bool authorized,
                                  bool cancel, uint32_t cause, bool stop, bool capture_fault,
                                  bool drained, uint64_t now, uint64_t count, uint64_t last_edge,
                                  uint32_t direction_epoch, uint32_t invalid_transitions,
                                  bool negative, int64_t position, const uint32_t configuration[20],
                                  uint32_t reply[40]) {
    static dmotor::Dispatcher dispatcher;
    const dmotor::EncoderSample encoder{count, last_edge, direction_epoch, invalid_transitions,
                                        negative};
    dispatcher.service(reset, request, tick, authorized, cancel, cause, stop, capture_fault,
                       drained, now, encoder, position,
                       dmotor::decode_controller_config(configuration), reply);
}
