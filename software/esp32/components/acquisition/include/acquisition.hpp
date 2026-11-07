#pragma once
#include <array>
#include <cstdint>
#include <limits>

namespace dmotor_monitor {
using Us = std::int64_t;
struct Clock { virtual Us now() = 0; virtual ~Clock() = default; };
struct Bus {
    // Synchronous, single owner. No retries or asynchronous callbacks.
    virtual bool read(unsigned device, std::uint8_t reg, int timeout_ms,
                      std::uint16_t& value) = 0;
    virtual ~Bus() = default;
};
enum class Status { complete, config, deadline, unrepresentable, io };
struct Result {
    Status status = Status::config;
    unsigned reads = 0;
    std::array<std::array<std::uint16_t, 3>, 4> raw{};
    Us release = 0, deadline = 0, finished = 0;
    // Transport completion is NOT conversion freshness or protection approval.
};
// Floor to whole ticks; never round up beyond the remaining time. Refuse zero.
// Supported tick frequencies divide 1000 exactly. IDF receives integer ms.
inline int timeout_ms(Us remaining, unsigned hz, unsigned cap_ms = 2) {
    if ((hz != 100 && hz != 1000) || remaining <= 0 || cap_ms == 0) return 0;
    const Us tick = 1000000 / hz;
    const Us ticks = (remaining < static_cast<Us>(cap_ms) * 1000
                       ? remaining : static_cast<Us>(cap_ms) * 1000) / tick;
    return static_cast<int>(ticks * (1000 / hz));
}
struct Config {
    unsigned population = 15; // inhibited-only installed mask, never discovery
    unsigned tick_hz = 1000;
    Us ina_start_us = 1000, ina_end_us = 8000; // relative to scheduled release
};
class Acquisition {
    Clock& clock_; Bus& bus_;
    bool faulted_ = false;
public:
    Acquisition(Clock& clock, Bus& bus) : clock_(clock), bus_(bus) {}
    bool faulted() const { return faulted_; }
    // Fault latch deliberately has no automatic clear/rearm API.
    Result run(Us release, Config c = {}) {
        Result r; r.release = release;
        auto fail = [&](Status s) { r.status = s; r.raw = {}; faulted_ = true;
                                   r.finished = clock_.now(); return r; };
        if (release < 0 || release > std::numeric_limits<Us>::max() - 10000 ||
            c.population == 0 || c.population > 15 || c.ina_start_us < 0 ||
            c.ina_end_us <= c.ina_start_us || c.ina_end_us > 8000 ||
            (c.tick_hz != 100 && c.tick_hz != 1000)) return fail(Status::config);
        r.deadline = release + c.ina_end_us;
        Us last = clock_.now();
        if (last < release + c.ina_start_us || last >= r.deadline)
            return fail(Status::deadline);
        constexpr std::uint8_t regs[] = {0x06, 0x01, 0x02};
        for (unsigned d = 0; d < 4; ++d) {
            if (!(c.population & (1u << d))) continue;
            for (unsigned j = 0; j < 3; ++j) {
                const Us before = clock_.now();
                if (before < last || before >= r.deadline) return fail(Status::deadline);
                const int wait = timeout_ms(r.deadline - before, c.tick_hz);
                if (!wait) return fail(Status::unrepresentable);
                std::uint16_t value = 0;
                const bool ok = bus_.read(d, regs[j], wait, value);
                const Us after = clock_.now();
                if (after < before || after >= r.deadline) return fail(Status::deadline);
                if (!ok) return fail(Status::io);
                r.raw[d][j] = value; ++r.reads; last = after;
            }
        }
        r.finished = clock_.now();
        if (r.finished < last || r.finished >= r.deadline) return fail(Status::deadline);
        r.status = Status::complete;
        return r;
    }
};
// Necessary condition only. Caller must independently prove ADC completeness,
// INA clear/ready history, age, calibration, policy, recovery and ISR fault epoch.
// Use at final serialized output commit, never cache a true value.
inline bool acquisition_qualified(const Result& r, Us now, bool latched_fault,
                                  bool other_safety_proofs) {
    return r.status == Status::complete && !latched_fault && other_safety_proofs &&
           now >= r.finished && now < r.deadline;
}
} // namespace dmotor_monitor
