#include "timing_fixture.hpp"
#include "dmotor/motion.hpp"
#include <cstdint>
#include <cstdio>
int main() {
    uint32_t state = 0x12345678;
    auto random = [&]() {
        state = state * 1664525U + 1013904223U;
        return state;
    };
    uint32_t command[16] = {}, reply[16] = {};
    const uint32_t gains[] = {0, 1, 499, 500, 999, 1000, 65536, 1048576};
    const uint32_t numerators[] = {1, 10296, 1000000, 1000000};
    const uint32_t denominators[] = {1, 5, 167, 1000000};
    for (unsigned profile = 0; profile < 128; ++profile) {
        uint32_t c[16] = {
            1,   1000000, 1 + random() % 10000000, 1 + random() % 10000000,  10296, 5,     10,
            100, 2,       gains[profile % 8],      gains[(profile / 8) % 8], 65536, 65536, 1000000,
            0,   0};
        c[4] = numerators[profile % 4];
        c[5] = denominators[profile % 4];
        dmotor_motion(true, false, false, command, c, false, false, 1, 0, 0, 0, false, reply);
        for (unsigned tick = 1; tick <= 128; ++tick) {
            for (auto& w : command) {
                w = 0;
            }
            command[0] = 1;
            command[3] = command[4] = 1;
            int32_t target = static_cast<int32_t>(random() % 1998001) - 999000;
            if (target < 1000 && target > -1000) {
                target = 1000;
            }
            command[5] = static_cast<uint32_t>(target);
            if (profile >= 64) {
                command[0] = 1 + profile % 6;
                if (command[0] == 2 || command[0] == 3) {
                    command[5] = 130000;
                    command[6] = profile % 2 ? 100 : static_cast<uint32_t>(-100);
                    command[7] = profile % 2 ? 0 : UINT32_MAX;
                    command[11] = 30000;
                } else if (command[0] == 4) {
                    command[5] = 0;
                    command[8] = static_cast<uint32_t>(target % 65537);
                } else if (command[0] == 5) {
                    command[5] = 0;
                    command[9] = 1500;
                } else if (command[0] == 6) {
                    command[5] = 0;
                    command[11] = 30000;
                }
            }
            int32_t measured = static_cast<int32_t>(random() % 2000001) - 1000000;
            dmotor_motion(false, true, true, command, c, true, false, 1, uint64_t(tick) * test_timing::tick_cycles, 0,
                          measured, true, reply);
            for (auto w : reply) {
                std::printf("%08x", w);
            }
            std::puts("");
        }
    }
    // Long settling crosses the 19-bit boundary and completes at the maximum
    // accepted 20-bit count. Run in native comparison, not million-step cosim.
    uint32_t c[16] = {1, 1000000, 10000000, 10000000, 1, 1, 0, 0,
                      1000000, 0, 0, 65536, 65536, 2000000000, 0, 0};
    for (auto& word : command) {
        word = 0;
    }
    command[0] = 6;
    command[3] = command[4] = 1;
    command[11] = 2000000000;
    dmotor_motion(true, false, false, command, c, false, false, 1, 0, 0, 0, false, reply);
    for (unsigned tick = 1; tick <= 1000000; ++tick) {
        dmotor_motion(false, true, tick == 1, command, c, false, false, 1,
                      uint64_t(tick) * test_timing::tick_cycles, 0, 0, true, reply);
        const bool busy = (reply[2] & 2) != 0;
        if (reply[7] || busy != (tick < 1000000) ||
            (tick == 1000000 && !(reply[2] & 4))) {
            return 1;
        }
        if (tick >= 999998) {
            for (auto w : reply) {
                std::printf("%08x", w);
            }
            std::puts("");
        }
    }
}
