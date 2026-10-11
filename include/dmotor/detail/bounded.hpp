#pragma once
#include <cstdint>
namespace dmotor {
namespace detail {
// Explicit sign extension exposes private value widths to HLS while retaining
// scalar C++ storage. Call only AFTER validation/clamping proves the bound.
// No signed shifts, overflow, or out-of-range unsigned-to-signed conversions.
template <unsigned Bits> int64_t narrow_signed(int64_t value) {
    static_assert(Bits > 1 && Bits < 64, "invalid signed width");
    constexpr uint64_t sign = uint64_t{1} << (Bits - 1);
    const uint64_t raw = static_cast<uint64_t>(value);
    return static_cast<int64_t>(raw & (sign - 1)) - static_cast<int64_t>(raw & sign);
}
} // namespace detail
} // namespace dmotor
