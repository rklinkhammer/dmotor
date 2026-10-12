#include "dmotor/detail/arithmetic.hpp"
namespace dmotor {
namespace detail {
uint64_t divide(uint64_t numerator, uint64_t denominator) {
    // Shift dividend bits out of the high end and quotient bits into the low
    // end. After 64 iterations this same register contains the full quotient.
    uint64_t dividend_quotient = numerator, remainder = 0;
    bool subtract = false;
    for (unsigned phase = 0; phase < 128; ++phase) {
        if ((phase & 1U) == 0) {
            const bool carry = (remainder >> 63) != 0;
            remainder = (remainder << 1) | (dividend_quotient >> 63);
            subtract = carry || remainder >= denominator;
        } else {
            if (subtract) {
                remainder -= denominator;
            }
            dividend_quotient = (dividend_quotient << 1) | static_cast<uint64_t>(subtract);
        }
    }
    return dividend_quotient;
}

} // namespace detail
} // namespace dmotor
