#pragma once
#include <cstdint>
namespace dmotor {
namespace detail {
// Exact unsigned division; denominator must be nonzero. One restoring step
// per bit combines comparison and subtraction; preserves all 64 input bits.
uint64_t divide(uint64_t numerator, uint64_t denominator);
// Bounded PI only: numerator < 2^40, 0 < denominator < 2^20.
// Callers include rounding offsets before entry and prove these bounds.
uint64_t divide_pi(uint64_t numerator, uint32_t denominator);
} // namespace detail
} // namespace dmotor
