#include "dmotor/detail/arithmetic.hpp"
#include "dmotor/capabilities.hpp"
#if defined(__SYNTHESIS__) || defined(DMOTOR_USE_AP_INT)
#include <ap_int.h>
#endif
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

uint64_t divide_pi(uint64_t numerator, uint32_t denominator) {
    constexpr unsigned numerator_bits = capability::pi_numerator_bits;
    constexpr unsigned denominator_bits = capability::pi_denominator_bits;
    constexpr uint64_t numerator_mask = (uint64_t{1} << numerator_bits) - 1;
    constexpr uint32_t denominator_mask = (uint32_t{1} << denominator_bits) - 1;
#if defined(__SYNTHESIS__) || defined(DMOTOR_USE_AP_INT)
    using quotient_type = ap_uint<numerator_bits>;
    using remainder_type = ap_uint<denominator_bits>;
#else
    // The masked native model follows the same per-phase truncation as HLS.
    using quotient_type = uint64_t;
    using remainder_type = uint32_t;
#endif
    quotient_type quotient = numerator & numerator_mask;
    remainder_type remainder = 0;
    const remainder_type divisor = denominator & denominator_mask;
    // One restoring step per dividend bit. Carry preserves the extra bit
    // while the stored remainder stays denominator-sized.
    for (unsigned bit = 0; bit < numerator_bits; ++bit) {
        const bool carry = (remainder >> (denominator_bits - 1)) != 0;
        remainder = ((remainder << 1) |
                     static_cast<remainder_type>(quotient >> (numerator_bits - 1))) &
                    denominator_mask;
        const bool subtract = carry || remainder >= divisor;
        if (subtract) remainder = (remainder - divisor) & denominator_mask;
        quotient = ((quotient << 1) | static_cast<quotient_type>(subtract)) & numerator_mask;
    }
    return static_cast<uint64_t>(quotient);
}

} // namespace detail
} // namespace dmotor
