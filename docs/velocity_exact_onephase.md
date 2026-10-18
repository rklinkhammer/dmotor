# One-phase shared exact PI divider

> Current product builds: [build guide](build.md). Commands below are specialized
> component/experiment reproduction or historical evidence, not the default build.


This experiment replaces only the bounded velocity candidate's 39/20-bit PI
divider loop. Each iteration shifts the next numerator bit into the remainder,
compares/subtracts, and advances the quotient in one phase. The remainder's
shifted-out carry is still retained, so the operation remains exact for every
nonzero 20-bit divisor and 39-bit numerator. There are 39 iterations rather than
78 alternating phases. The general encoder divider is unchanged.

The candidate lives in `build/velocity_exact_onephase`. Main full-profile sources
and the previously measured two-phase candidate remain unchanged.

## Measured comparison

Vitis 2026.1, xc7z020clg400-1, 20 MHz; same candidate, inputs, constraints, gains,
precision, sharing directives and test benches. These are integrated HLS estimates,
not post-route utilization.

| Metric | Two phases per bit | One phase per bit | Difference |
|---|---:|---:|---:|
| Integrated PI divider LUT | 225 | 153 | -72 |
| Integrated PI divider FF | 70 | 68 | -2 |
| Integrated PI divider cycles | 80 | 41 | -39 |
| Integrated PI divider estimated path | 4.492 ns | 5.470 ns | +0.978 ns |
| Dispatcher LUT | 21007 | 20935 | -72 (-0.34%) |
| Dispatcher FF | 10443 | 10441 | -2 |
| Dispatcher DSP | 20 | 20 | unchanged |
| Dispatcher BRAM18K | 4 | 4 | unchanged |
| Dispatcher maximum cycles | 651 | 534 | -117 |
| Dispatcher maximum latency | 32.55 us | 26.70 us | -5.85 us |
| Dispatcher estimated critical path | 34.617 ns | 34.617 ns | unchanged |
| Motion kernel LUT | 10978 | 10906 | -72 |
| Motion kernel FF | 5322 | 5320 | -2 |
| Motion kernel maximum cycles | 339 | 222 | -117 |

The PI divider uses no DSP or BRAM in either version. Its longer combinational
step fits comfortably within the 36.50 ns effective HLS budget, and does not
become the dispatcher critical path. The dispatcher serializes three rounded
PI operations, explaining the three-times-39-cycle worst-case reduction.
The motion kernel retains 13 DSP, no BRAM, and a 35.742 ns estimated path.

## Verification

The 14 applicable native/RTL tests pass with the existing UBSan build. The
bounded arithmetic test passes 100000 random nonzero-divisor pairs and boundary
cases with both native masks and Vitis arbitrary-width integer types. All 4451
motion, 990 controller and 1251 dispatcher retained-profile reply records match
the original implementation exactly. Thus this experiment changes scheduling
and hardware, not numerical precision or rounding semantics.

Reproduction (fresh output directory):

```sh
python3 tools/prepare_bounded_velocity.py --variant exact-onephase --output build/onephase_new
python3 build/onephase_new/tools/vitis_check.py --kernel motion
python3 build/onephase_new/tools/vitis_check.py --kernel dispatcher
```

The helper reproduces `src/arithmetic.cpp` byte-for-byte. The integration patch
is against `exact-shared`, and is preserved with reports and logs in
`docs/evidence/velocity_exact_onephase/`.

## Serialized DSP reciprocal assessment (not synthesized here)

A serialized reciprocal remains possible, but the measured exact PI divider now
costs only 153 LUTs (about 0.73% of the dispatcher). A replacement must beat its
*complete* cost, including operand selection, accumulation, shifts, correction
and sequencing. Spare DSPs alone do not establish a LUT benefit. Neither a
six-DSP allocation nor a LUT saving is assumed. A throughput of one result per
fabric clock is unnecessary for the 1 kHz control loop.

For this PI path, generic Newton-Raphson is also unnecessary: the only divisors
are 1000 and 1000000. A more focused future candidate would use precomputed
reciprocals and a serialized multiplier, with exact quotient correction:

- For numerator n < 2^39, choose m = ceil(2^39 / d).
- Estimate q = floor(n * m / 2^39).
- If q * d > n, decrement q once.

Since 0 <= m/2^39 - 1/d < 1/2^39, the estimate is at most one above the correct
floor quotient. The single correction therefore preserves exact division; the
existing signed-rounding wrapper can stay unchanged. For d=1000 the reciprocal
has 30 bits, so n*m needs up to 69 bits; for d=1000000 it has 20 bits, requiring
up to 59 product bits. These are not generic 25-bit multiplications. Explicit
multi-cycle partial-product accumulation could share multiplier hardware, but
its accumulator/control overhead must be measured. Such a specialization would
also need a stricter divisor contract and dedicated tests; the current helper
supports every valid nonzero 20-bit divisor.

Recommendation: prefer the measured one-phase exact PI candidate. Keep a DSP
reciprocal as a lower-priority experiment, with the 153-LUT core and 20935-LUT
complete dispatcher as the new baselines. The larger encoder conversion and
shared-motor architecture are separate optimization targets. No reciprocal
performance/resource claim or complete-board qualification is made here.

Motion and complete dispatcher passed Vitis C simulation, synthesis and Verilog
co-simulation with arbitrary-width arithmetic enabled. Generated RTL checks
confirm one shared PI divider and a separate wide estimator divider; instance
evidence is in `hierarchy.txt`.
