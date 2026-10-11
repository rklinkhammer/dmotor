#include "dmotor/detail/arithmetic.hpp"
namespace dmotor {
namespace detail {
uint64_t divide(uint64_t numerator, uint64_t denominator) {
    uint64_t quotient = 0, remainder = 0;
    bool subtract = false;
    for (unsigned phase = 0; phase < 128; ++phase) {
        if ((phase & 1U) == 0) {
            const bool carry = (remainder >> 63) != 0;
            remainder = (remainder << 1) | (numerator >> 63);
            numerator <<= 1;
            subtract = carry || remainder >= denominator;
        } else {
            if (subtract) {
                remainder -= denominator;
            }
            quotient = (quotient << 1) | static_cast<uint64_t>(subtract);
        }
    }
    return quotient;
}

} // namespace detail
} // namespace dmotor
