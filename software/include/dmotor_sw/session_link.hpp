#pragma once
#include "dmotor_sw/monitor_protocol.hpp"
#include <array>
namespace dmotor_sw {
namespace monitor {
enum class Delivery {
    rejected,
    discovery,
    paired,
    accepted,
    duplicate,
    re_pair_required,
    exhausted
};
struct Reception {
    Delivery delivery = Delivery::rejected;
    bool respond = false;
    Frame response{};
};
// Isolated symmetric link model. Confirmed IDs are not motion/ARM permission.
// The caller supplies durably allocated lifetimes and enforces real inhibition.
class SessionLink {
  public:
    explicit SessionLink(uint32_t transmit_budget = UINT32_MAX - 1);
    Frame begin(uint64_t fresh_local_session, bool inhibited);
    Reception receive(const Frame& frame);
    bool send(Frame& frame); // Assign identity/sequence to a new control frame.
    bool remember_reply(const Frame& request, const Frame& response);
    bool paired() const {
        return paired_;
    }
    uint64_t local_session() const {
        return local_;
    }
    uint64_t peer_session() const {
        return paired_ ? peer_ : 0;
    }
    void invalidate(); // Caller must also inhibit/abort recovery.

  private:
    struct Entry {
        bool used = false, has_reply = false;
        Frame request{}, reply{};
    };
    bool number(Frame& frame);
    void cache(const Frame& frame);
    static bool same(const Frame& a, const Frame& b);
    Reception replay(const Frame& frame) const;
    Reception repair(uint64_t peer);
    uint64_t local_ = 0, peer_ = 0, peer_floor_ = 0;
    uint32_t tx_ = 0, rx_ = 0, budget_;
    bool discovering_ = false, paired_ = false, saw_hello_ = false;
    Frame hello_{}, ack_{};
    std::array<Entry, 4> history_{};
    std::size_t next_ = 0;
};
} // namespace monitor
} // namespace dmotor_sw
