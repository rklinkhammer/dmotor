#pragma once
#include <cstdint>
namespace dmotor {
// Shared submission/completion capacity. After changing this value run:
// python3 tools/generate_abi.py
constexpr uint32_t ring_depth = 4;
static_assert(ring_depth >= 2 && ring_depth <= 64 && (ring_depth & (ring_depth - 1)) == 0,
              "ring_depth must be a power of two from 2 to 64");
constexpr uint32_t ring_mask = ring_depth - 1;
} // namespace dmotor
