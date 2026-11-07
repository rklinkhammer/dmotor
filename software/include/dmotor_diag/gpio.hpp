#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace dmotor_diag {
enum class BenchCommand { none, gpio, quiet, hb10, stop, help, invalid };
// Bounded line parser: malformed/overlong input is discarded through the delimiter.
class BenchConsole {
  public:
    BenchCommand push(char c) {
        if (c == '\r' || c == '\n') {
            BenchCommand command = BenchCommand::none;
            if (overflow_) {
                command = BenchCommand::invalid;
            } else if (used_) {
                text_[used_] = 0;
                command = std::strcmp(text_, "gpio") == 0    ? BenchCommand::gpio
                          : std::strcmp(text_, "quiet") == 0 ? BenchCommand::quiet
                          : std::strcmp(text_, "hb10") == 0  ? BenchCommand::hb10
                          : std::strcmp(text_, "stop") == 0  ? BenchCommand::stop
                          : std::strcmp(text_, "help") == 0  ? BenchCommand::help
                                                             : BenchCommand::invalid;
            }
            used_ = 0;
            overflow_ = false;
            return command;
        }
        if (c < 32 || c > 126 || used_ == sizeof(text_) - 1) {
            overflow_ = true;
        } else if (!overflow_) {
            text_[used_++] = c;
        }
        return BenchCommand::none;
    }

  private:
    char text_[16]{};
    std::size_t used_ = 0;
    bool overflow_ = false;
};
enum class HbEnd : uint32_t { idle, running, stopped, expired, disconnected, late };
// Synthetic bench heartbeat only. No motor permission/ARM authority exists here.
class BenchHeartbeat {
  public:
    static constexpr uint64_t duration_us = 10000000;
    static constexpr uint64_t half_period_us = 10000;
    bool start(uint64_t requested, uint64_t now, bool connected) {
        // Reject delayed/replayed commands; repeated starts never extend an active run.
        if (active_ || !connected || now < requested || now - requested >= half_period_us) {
            return false;
        }
        active_ = true;
        high_ = false;
        end_ = requested + duration_us;
        next_ = now + half_period_us;
        reason_ = HbEnd::running;
        return true;
    }
    void stop(HbEnd reason = HbEnd::stopped) {
        active_ = high_ = false;
        reason_ = reason;
    }
    void step(uint64_t now, bool connected) {
        if (!active_) {
            return;
        }
        if (!connected) {
            stop(HbEnd::disconnected);
        } else if (now >= end_) {
            stop(HbEnd::expired);
        } else if (now >= next_) {
            if (now - next_ >= half_period_us) {
                stop(HbEnd::late); // Never replay missed edges or emit catch-up pulses.
            } else {
                high_ = !high_;
                next_ += half_period_us;
            }
        }
    }
    bool high() const {
        return high_;
    }
    bool active() const {
        return active_;
    }
    HbEnd reason() const {
        return reason_;
    }
    uint64_t remaining(uint64_t now) const {
        return active_ && now < end_ ? end_ - now : 0;
    }

  private:
    bool active_ = false, high_ = false;
    uint64_t end_ = 0, next_ = 0;
    HbEnd reason_ = HbEnd::idle;
};
} // namespace dmotor_diag
