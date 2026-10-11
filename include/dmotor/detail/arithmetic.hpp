#pragma once
#include <cstdint>
namespace dmotor {
namespace detail {
// Exact unsigned division; denominator must be nonzero. Two phases per bit
// separate comparison from subtraction, bounding the combinational path.
uint64_t divide(uint64_t numerator, uint64_t denominator);
} // namespace detail
} // namespace dmotor
