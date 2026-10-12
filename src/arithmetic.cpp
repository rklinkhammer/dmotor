#include "dmotor/detail/arithmetic.hpp"
namespace dmotor {
namespace detail {
uint64_t divide(uint64_t numerator, uint64_t denominator) {
    // Shift dividend bits out of the high end and quotient bits into the low
    // end. After 64 iterations this same register contains the full quotient.
    uint64_t dividend_quotient = numerator, remainder = 0;
    // One restoring step per dividend bit; retain full-width callers such
    // as encoder scaling, elapsed time and position braking calculations.
    constexpr unsigned dividend_bits = 64;
    for (unsigned bit = 0; bit < dividend_bits; ++bit) {
        const bool carry = (remainder >> (dividend_bits - 1)) != 0;
        remainder = (remainder << 1) | (dividend_quotient >> (dividend_bits - 1));
        const bool subtract = carry || remainder >= denominator;
        if (subtract) {
            remainder -= denominator;
        }
        dividend_quotient = (dividend_quotient << 1) | static_cast<uint64_t>(subtract);
    }
    return dividend_quotient;
}

} // namespace detail
} // namespace dmotor
