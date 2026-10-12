#include "divider.hpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#ifndef DIVIDER_WIDTH
#error DIVIDER_WIDTH required
#endif
#ifndef DENOMINATOR_WIDTH
#define DENOMINATOR_WIDTH DIVIDER_WIDTH
#endif
#ifdef ORIGINAL_DIVIDER
#include "dmotor/detail/arithmetic.hpp"
#else
ap_uint<DIVIDER_WIDTH> divider_top(ap_uint<DIVIDER_WIDTH>, ap_uint<DENOMINATOR_WIDTH>);
#endif
static constexpr uint64_t mask = UINT64_MAX >> (64 - DIVIDER_WIDTH);
static void check(uint64_t n, uint64_t d) {
    n &= mask;
    d &= UINT64_MAX >> (64 - DENOMINATOR_WIDTH);
    if (!d) return;
#ifdef ORIGINAL_DIVIDER
    const uint64_t got = dmotor::detail::divide(n, d);
#else
    const uint64_t got = divider_top(n, d).to_uint64();
#endif
    if (got != n / d) {
        std::cerr << n << '/' << d << ": " << got << " != " << n / d << '\n';
        std::exit(1);
    }
}
int main() {
    const uint64_t ends[] = {0, 1, 2, 3, mask / 2, mask / 2 + 1, mask - 1, mask};
    for (auto n : ends) for (auto d : ends) check(n, d);
    for (unsigned bit = 0; bit < DIVIDER_WIDTH; ++bit) {
        const uint64_t power = uint64_t{1} << bit;
        check(mask, power); check(power - 1, power);
        check(power, power); check(power + 1, power);
    }
    uint64_t state = 0x106689d45497fdb5ULL;
#ifdef EXTENDED_TEST
    constexpr unsigned random_pairs = 100000;
#else
    constexpr unsigned random_pairs = 256;
#endif
    for (unsigned i = 0; i < random_pairs; ++i) {
        state = state * 6364136223846793005ULL + 1;
        const uint64_t n = state;
        state = state * 6364136223846793005ULL + 1;
        check(n, (state & mask) ? state : 1);
    }
    std::cout << DIVIDER_WIDTH << "-bit divider: boundaries and " << random_pairs
              << " random pairs passed\n";
}
