#pragma once
#include "dmotor_sw/runtime.hpp"
#include "dmotor/controller_registers.hpp"
#include <map>
#include <functional>
#include <vector>
// Test includer supplies CHECK and namespace aliases.
struct Fake : Platform {
    std::map<uint32_t, uint32_t> words;
    std::vector<std::pair<uint32_t, uint32_t>> writes;
    uint64_t time = 0;
    std::function<void(uint32_t)> after_read;
    bool source = true, reset = true, fail = false, hold_ready = false, halt_failure = false;
    unsigned halt_count = 0, cleanup_failure = 0, cleanup_reads = 0;
    Fake() {
        words[reg::CLEANUP_CAPABILITY] = 0x434c4e31;
        words[reg::MONITOR_CAPABILITY] = 0x48423131;
        words[reg::MONITOR_STATUS] = 5;
        words[reg::DIRECT_CAPABILITY] = 0x44445531;
        words[reg::IDENTITY] = reg::PROFILE_ID;
        words[reg::STATUS] = 1U << 18;
        words[b::IDENTITY] = b::PROFILE_ID;
        words[b::SCHEMA] = 1;
        words[b::MOTOR_COUNT] = 4;
    }
    uint32_t read32(uint32_t a) override {
        if (fail)
            throw IoError("bus");
        if (a == reg::CLEANUP_STATUS && cleanup_failure == 5 && ++cleanup_reads > 1)
            return 4; // A new fault between the first proof read and final check.
        const auto result = words[a];
        if (after_read)
            after_read(a);
        return result;
    }
    void write32(uint32_t a, uint32_t v) override {
        if (fail)
            throw IoError("bus");
        writes.emplace_back(a, v);
        if (a == reg::MONITOR_ACK) {
            CHECK(reset && !(words[reg::STATUS] & 15));
            if (words[reg::MONITOR_STATUS] & 1U)
                words[reg::MONITOR_STATUS] &= ~2U;
        }
        if (a == reg::SESSION) {
            CHECK(reset && !(words[reg::STATUS] & 15));
            words[b::SESSION] = v;
        }
        if (a == reg::CORE_RESET) {
            reset = v != 0;
            words[reg::CLEANUP_STATUS] = reset ? 0 : 2;
            words[reg::CLEANUP_SESSION] = reset ? 0 : words[reg::SESSION];
            if (!reset)
                ++words[reg::CLEANUP_REQUEST];
            if (!reset) {
                if (cleanup_failure == 1)
                    words[reg::CLEANUP_STATUS] = 4;
                if (cleanup_failure == 2)
                    words[reg::CLEANUP_STATUS] = 1;
                if (cleanup_failure == 3)
                    words[reg::CLEANUP_SESSION] = 0;
                if (cleanup_failure == 4)
                    --words[reg::CLEANUP_REQUEST];
            }
            if (reset)
                words[reg::STATUS] |= 1U << 18;
            else if (!source)
                words[reg::STATUS] &= ~(1U << 18);
        }
        if (a == reg::ENABLE) {
            CHECK(!reset && !source);
            words[reg::STATUS] |= v;
            if (!hold_ready)
                words[reg::STATUS] |= v << 8;
        }
        if (a == reg::DISABLE)
            words[reg::STATUS] &= ~(v | (v << 8));
        if (a == reg::STOP && words[reg::DIRECT_MODE])
            words[reg::STATUS] &= ~(v | (v << 8));
        if (a == reg::STOP) {
            words[reg::STATUS] |= v << 12;
            words[reg::STATUS] &= ~(v << 8);
            words[b::PENDING_MASK] = 0;
        }
        if (a == b::COMMIT) {
            CHECK(v == 1 && !source && !reset);
            words[b::PUBLISHED_SEQUENCE] = words[b::STAGE_SEQUENCE];
            words[b::APPLIED_SEQUENCE] = words[b::STAGE_SEQUENCE];
            words[b::APPLIED_MASK] = words[b::STAGE_MASK];
            words[b::PENDING_MASK] = 0;
        }
        if (a == reg::ACKNOWLEDGE && !source)
            words[reg::STATUS] &= ~((1U << 16) | (1U << 17));
        words[a] = v;
    }
    void fence() override {
        if (fail)
            throw IoError("fence");
    }
    void halt(bool asserted) override {
        if (halt_failure)
            throw IoError("HALT confirmation unavailable");
        source = asserted;
        if (asserted) {
            ++halt_count;
            words[reg::STATUS] &= ~15U;
            words[reg::STATUS] |= 1U << 18;
        }
    }
    uint64_t now_us() const override {
        return time;
    }
    void sleep_us(uint32_t us) override {
        time += us;
    }
};
std::array<MotorConfiguration, 4> config() {
    std::array<MotorConfiguration, 4> c{};
    for (auto& m : c)
        m = {7,   400000, 1000000, 2000000, 1560,    1,       1,       100, 2,
             655, 65536,  32768,   16384,   1000000, 2000000, 1000000, 8192};
    return c;
}
template <class F> bool rejects(F f) {
    try {
        f();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}
