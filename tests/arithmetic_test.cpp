#include "dmotor/detail/arithmetic.hpp"
#include <cstdlib>
#include <iostream>
static void check(uint64_t numerator, uint64_t denominator) {
    if (dmotor::detail::divide(numerator, denominator) != numerator / denominator) {
        std::cerr << "division mismatch: " << numerator << " / " << denominator << '\n';
        std::exit(1);
    }
}
int main() {
    const uint64_t limits[] = {0, 1, 2, 3, 999, 1000, 1000000000, UINT32_MAX,
                               uint64_t{1} << 32, (uint64_t{1} << 63) - 1,
                               uint64_t{1} << 63, UINT64_MAX - 1, UINT64_MAX};
    for (auto n : limits) {
        for (auto d : limits) {
            if (d) {
                check(n, d);
            }
        }
    }
    for (unsigned bit = 0; bit < 64; ++bit) {
        const uint64_t power = uint64_t{1} << bit;
        check(UINT64_MAX, power);
        check(power - 1, power);
        check(power, power);
        check(power + 1, power);
    }
    for (uint64_t n = 0; n < 256; ++n) {
        for (uint64_t d = 1; d < 256; ++d) {
            check(n, d);
        }
    }
    uint64_t state = 0x106689d45497fdb5ULL;
    for (unsigned i = 0; i < 100000; ++i) {
        state = state * 6364136223846793005ULL + 1;
        const uint64_t n = state;
        state = state * 6364136223846793005ULL + 1;
        check(n, state ? state : 1);
    }
    std::cout << "arithmetic: exact division, full-width boundaries and 100000 random pairs passed\n";
}
