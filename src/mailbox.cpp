#include "dmotor/mailbox.hpp"
namespace dmotor {
void VelocityMailbox::service(bool reset, const uint32_t request[4], bool tick,
    bool authorized, bool stop, bool capture_fault, uint32_t session, uint64_t now,
    const EncoderSample& encoder, const ControllerConfig& config, uint32_t reply[16]) {
    bool accepted = false;
    if (reset) {
        *this = VelocityMailbox{};
        controller_.step(true, false, false, now, encoder, 0, session, false,
                         Descriptor{}, true, false, config);
    } else {
        capture_pending_ = capture_pending_ || capture_fault;
        if (!authorized || capture_pending_ || last_.faults) {
            pending_ = false;
            written_ = 0;
            stop_pending_ = false;
            if (!authorized) stopping_ = false;
        } else if (stop) {
            pending_ = false;
            written_ = 0;
            stopping_ = true;
            stop_pending_ = true;
        } else if (!stopping_) {
            if (request[0] == 0) {
                accepted = true;
            } else if (request[0] == 1 && request[1] < 4 && request[3] == 0) {
                staged_[request[1]] = request[2];
                written_ = static_cast<uint8_t>(written_ | (1U << request[1]));
                accepted = true;
            } else if (request[0] == 2 && !request[1] && !request[2] && !request[3]) {
                const int32_t speed = signed_word(staged_[3]);
                const int64_t absolute = speed < 0 ? -static_cast<int64_t>(speed) : speed;
                if (written_ == 15 && session && staged_[0] == session &&
                    config.motion.generation && staged_[1] == config.motion.generation &&
                    staged_[2] > published_sequence_ &&
                    absolute <= config.motion.max_speed_mrpm) {
                    pending_session_ = staged_[0];
                    pending_generation_ = staged_[1];
                    pending_sequence_ = staged_[2];
                    pending_speed_ = speed;
                    published_sequence_ = staged_[2];
                    pending_ = true;
                    accepted = true;
                } else rejects_ |= 1;
                written_ = 0; // Every publication requires a complete new frame.
            } else rejects_ |= 2;
        }
        if (tick) {
            // Recheck ownership at activation, not only publication.
            if (pending_ && (pending_session_ != session ||
                             pending_generation_ != config.motion.generation)) {
                pending_ = false;
                rejects_ |= 4;
            }
            Descriptor command{};
            command[0] = 1;
            command[2] = pending_sequence_;
            command[3] = pending_session_;
            command[4] = pending_generation_;
            command[5] = static_cast<uint32_t>(pending_speed_);
            const bool offer = pending_ && authorized && !stopping_ && !capture_pending_;
            last_ = controller_.step(false, authorized, capture_pending_, now, encoder, 0,
                                     session, offer, command, true, stop_pending_, config);
            if (last_.motion.accepted) {
                if (!last_.faults && last_.motion.outcome == 0)
                    applied_sequence_ = pending_sequence_;
                else rejects_ |= 8;
                pending_ = false;
            }
            stop_pending_ = false;
            if (last_.faults) { pending_ = false; written_ = 0; }
            if (stopping_ && last_.motion.ready && last_.motion.ramp_mrpm == 0)
                stopping_ = false;
        }
    }
    reply[0] = accepted;
    reply[1] = pending_;
    reply[2] = applied_sequence_;
    reply[3] = pending_ ? pending_sequence_ : 0;
    reply[4] = authorized && !capture_pending_ ? static_cast<uint32_t>(last_.motion.duty_q16) : 0;
    reply[5] = static_cast<uint32_t>(last_.motion.ramp_mrpm);
    reply[6] = static_cast<uint32_t>(last_.velocity.mrpm);
    reply[7] = (last_.velocity.valid ? 1U : 0U) | (last_.velocity.timed_out ? 2U : 0U) |
               (last_.acquiring ? 4U : 0U) | (last_.warming_up ? 8U : 0U) |
               (stopping_ ? 16U : 0U);
    reply[8] = last_.faults | (capture_pending_ ? 0x80000000U : 0U);
    reply[9] = rejects_;
    reply[10] = static_cast<uint32_t>(last_.velocity.age);
    reply[11] = static_cast<uint32_t>(last_.velocity.age >> 32);
    reply[12] = published_sequence_;
    reply[13] = session;
    reply[14] = reply[15] = 0;
}
}
extern "C" void dmotor_mailbox(bool reset, const uint32_t request[4], bool tick,
    bool authorized, bool stop, bool capture_fault, uint32_t session, uint64_t now,
    uint64_t count, uint64_t last_edge, uint32_t direction_epoch,
    uint32_t invalid_transitions, bool negative, const uint32_t configuration[20],
    uint32_t reply[16]) {
    static dmotor::VelocityMailbox mailbox;
    const dmotor::EncoderSample sample{count,last_edge,direction_epoch,invalid_transitions,negative};
    mailbox.service(reset,request,tick,authorized,stop,capture_fault,session,now,sample,
                    dmotor::decode_controller_config(configuration),reply);
}
