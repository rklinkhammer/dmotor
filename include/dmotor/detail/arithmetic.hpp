#pragma once
#include <cstdint>
namespace dmotor {
namespace detail {
// Exact unsigned division; denominator must be nonzero. One restoring step
// per bit combines comparison and subtraction; preserves all 64 input bits.
uint64_t divide(uint64_t numerator, uint64_t denominator);
} // namespace detail
} // namespace dmotor
