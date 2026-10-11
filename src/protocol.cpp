#include "dmotor/protocol.hpp"
#include <limits>

namespace dmotor {
int32_t signed_word(uint32_t word) {
    return word <= INT32_MAX ? static_cast<int32_t>(word)
                             : static_cast<int32_t>(static_cast<int64_t>(word) - 4294967296LL);
}
int64_t signed_pair(uint32_t low, uint32_t high) {
    const uint64_t bits = (static_cast<uint64_t>(high) << 32) | low;
    return bits <= INT64_MAX ? static_cast<int64_t>(bits) : -1 - static_cast<int64_t>(~bits);
}
Error validate(const Descriptor& d, uint32_t sequence, uint32_t epoch, uint32_t generation,
               int64_t position, int32_t max_speed_mrpm) {
    if (d[0] < 1 || d[0] > 6) {
        return Error::opcode;
    }
    if (d[1] || d[12] || d[13] || d[14] || d[15]) {
        return Error::reserved;
    }
    if (d[2] != sequence) {
        return Error::sequence;
    }
    if (d[3] != epoch) {
        return Error::epoch;
    }
    if (d[4] != generation) {
        return Error::generation;
    }
    const int32_t speed = signed_word(d[5]);
    const int32_t duty = signed_word(d[8]);
    const bool position_mode = d[0] == 2 || d[0] == 3;
    if (max_speed_mrpm <= 0 || max_speed_mrpm > 1000000 || speed < -max_speed_mrpm ||
        speed > max_speed_mrpm || duty < -65536 || duty > 65536 || (position_mode && speed <= 0)) {
        return Error::range;
    }
    if ((!position_mode && (d[6] || d[7])) || (d[0] != 1 && !position_mode && d[5]) ||
        (d[0] != 4 && d[8]) || (d[0] != 5 && (d[9] || d[10])) ||
        (!position_mode && d[0] != 6 && d[11])) {
        return Error::reserved;
    }
    if ((d[0] == 5 && !d[9] && !d[10]) || ((position_mode || d[0] == 6) && !d[11])) {
        return Error::range;
    }
    const int64_t displacement = signed_pair(d[6], d[7]);
    if (d[0] == 3 && ((displacement > 0 && position > INT64_MAX - displacement) ||
                      (displacement < 0 && position < INT64_MIN - displacement))) {
        return Error::overflow;
    }
    return Error::none;
}
bool CommandRing::reopen(bool drained, uint32_t generation, uint32_t initial_counter) {
    if (!drained || admission_ || busy_ || producer_ != consumer_ || cancellation_.valid ||
        completion_producer_ != completion_consumer_ || epoch_ == UINT32_MAX) {
        return false;
    }
    ++epoch_; // No automatic token reuse. Exhaustion requires a platform cold-reset/drain.
    generation_ = generation;
    producer_ = consumer_ = initial_counter;
    completion_producer_ = completion_consumer_ = initial_counter;
    slots_ = {};
    admission_ = true;
    return true;
}
void CommandRing::quiesce() {
    admission_ = false;
}
bool CommandRing::stage(uint32_t slot, uint32_t word, uint32_t value) {
    if (!admission_ || slot >= ring_depth || word >= 16) {
        return false;
    }
    const uint32_t distance = (slot - (consumer_ & ring_mask)) & ring_mask;
    if (distance < producer_ - consumer_) {
        return false;
    }
    slots_[slot][word] = value;
    return true;
}
bool CommandRing::publish(uint32_t requested_producer, uint32_t session) {
    if (!admission_ || session != epoch_ ||
        requested_producer - producer_ > ring_depth - (producer_ - consumer_)) {
        return false;
    }
    producer_ = requested_producer;
    return true;
}
bool CommandRing::offer(Descriptor& descriptor) const {
    if (!admission_ || busy_ || producer_ == consumer_ ||
        completion_producer_ - completion_consumer_ == ring_depth) {
        return false;
    }
    descriptor = slots_[consumer_ & ring_mask];
    return true;
}
// Standalone capture validates its active copy. Keep this path separate from
// capture_checked to avoid an extra descriptor read/copy in standalone HLS builds.
bool CommandRing::capture(uint64_t now, int64_t position, int32_t max_speed_mrpm) {
    if (!admission_ || busy_ || producer_ == consumer_ ||
        completion_producer_ - completion_consumer_ == ring_depth) {
        return false;
    }
    active_ = slots_[consumer_ & ring_mask];
    const Error error = validate(active_, consumer_, epoch_, generation_, position, max_speed_mrpm);
    // A rejection must correlate to the actual queue position, even if its wire
    // sequence or epoch is corrupt. Copy only; software can never mutate active_.
    active_[2] = consumer_;
    active_[3] = epoch_;
    busy_ = true;
    activation_ = error == Error::none ? now : 0;
    ++consumer_;
    if (error != Error::none) {
        finish(now, 1, static_cast<uint32_t>(error));
    }
    return true;
}
bool CommandRing::capture_checked(uint64_t now, Error error) {
    if (!admission_ || busy_ || producer_ == consumer_ ||
        completion_producer_ - completion_consumer_ == ring_depth) {
        return false;
    }
    active_ = slots_[consumer_ & ring_mask];
    // A rejection must correlate to the actual queue position, even if its wire
    // sequence or epoch is corrupt. Copy only; software can never mutate active_.
    active_[2] = consumer_;
    active_[3] = epoch_;
    busy_ = true;
    activation_ = error == Error::none ? now : 0;
    ++consumer_;
    if (error != Error::none) {
        finish(now, 1, static_cast<uint32_t>(error));
    }
    return true;
}
bool CommandRing::finish(uint64_t now, uint32_t outcome, uint32_t detail) {
    if (!busy_ || outcome > 2) {
        return false;
    }
    completions_[completion_producer_ & ring_mask] = {active_[2],
                                                      active_[3],
                                                      outcome,
                                                      detail,
                                                      static_cast<uint32_t>(activation_),
                                                      static_cast<uint32_t>(activation_ >> 32),
                                                      static_cast<uint32_t>(now),
                                                      static_cast<uint32_t>(now >> 32)};
    ++completion_producer_;
    busy_ = false;
    return true;
}
void CommandRing::cancel(uint64_t now, uint32_t cause) {
    admission_ = false;
    if (!cancellation_.valid) {
        cancellation_ = {true,      busy_,     epoch_, busy_ ? active_[2] : 0,
                         consumer_, producer_, cause,  now};
    }
    if (busy_) {
        finish(now, 2, cause);
    }
    consumer_ = producer_;
}
bool CommandRing::acknowledge_completions(uint32_t requested_consumer) {
    if (requested_consumer - completion_consumer_ > completion_producer_ - completion_consumer_) {
        return false;
    }
    completion_consumer_ = requested_consumer;
    return true;
}
void CommandRing::acknowledge_cancellation() {
    cancellation_.valid = false;
}
bool CommandRing::read_completion(uint32_t counter, Completion& result) const {
    if (counter - completion_consumer_ >= completion_producer_ - completion_consumer_) {
        return false;
    }
    result = completions_[counter & ring_mask];
    return true;
}
const Descriptor& CommandRing::active() const {
    return active_;
}
bool CommandRing::busy() const {
    return busy_;
}
bool CommandRing::admission_open() const {
    return admission_;
}
uint32_t CommandRing::producer() const {
    return producer_;
}
uint32_t CommandRing::consumer() const {
    return consumer_;
}
uint32_t CommandRing::completion_producer() const {
    return completion_producer_;
}
uint32_t CommandRing::epoch() const {
    return epoch_;
}
const Cancellation& CommandRing::cancellation() const {
    return cancellation_;
}
} // namespace dmotor
