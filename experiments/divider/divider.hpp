#pragma once
#include <ap_int.h>

// Same restoring algorithm as detail::divide, with every arithmetic register
// explicitly width-sized. Denominator must be nonzero. Unsigned quotient,
// floor rounding, two phases per dividend bit, including the carry bit.
template <int Width, int DenominatorWidth = Width>
ap_uint<Width> divide_sized(ap_uint<Width> numerator, ap_uint<DenominatorWidth> denominator) {
    static_assert(Width >= 2 && Width <= 64, "unsupported divider width");
    ap_uint<Width> dividend_quotient = numerator;
    static_assert(DenominatorWidth >= 2 && DenominatorWidth <= Width, "invalid denominator width");
    ap_uint<DenominatorWidth> remainder = 0;
    bool subtract = false;
    for (unsigned phase = 0; phase < 2 * Width; ++phase) {
        if ((phase & 1U) == 0) {
            const bool carry = remainder[DenominatorWidth - 1];
            remainder = (remainder << 1) | (dividend_quotient >> (Width - 1));
            subtract = carry || remainder >= denominator;
        } else {
            if (subtract) remainder -= denominator;
            dividend_quotient = (dividend_quotient << 1) | ap_uint<Width>(subtract);
        }
    }
    return dividend_quotient;
}
