# Width-specialized divider measurement

This isolated sweep compares the existing two-phase restoring divider with the
same algorithm using `ap_uint<Width>` for its numerator, denominator, remainder,
and combined dividend/quotient register. Each dividend bit takes two loop phases;
the remainder's shifted-out carry is retained for comparison. Denominators are
nonzero, division is unsigned, and results round down. Signed rounding remains a
caller responsibility and is not included in these standalone resource figures.

Source: `experiments/divider/divider.hpp`; runner:
`tools/measure_dividers.py`. Production arithmetic and all motor profiles remain
unchanged. The experiment uses xc7z020clg400-1, 20 MHz, Vitis 2026.1 and reset-all,
matching the earlier profile constraints. Width changes affect actual HLS types,
not just values sign-extended into 64-bit function arguments. A separate run of
the original `src/arithmetic.cpp` provides the direct reference.

## Widths required by the bounded PI candidates

Inputs are validated against +/-200 RPM before conversion. Gain bounds remain
0..1048576. The integral clamp is +/-65536 effort units. At 1 kHz the candidate
accumulates `Ki * error`, and divides by `velocity_units_per_RPM * 1000` when
producing effort. The numerator includes the rounding offset; widths below are
unsigned magnitudes, not signed state widths.

| Velocity units | Maximum absolute error | P numerator including rounding | P bits | I numerator including rounding | I bits | Shared P/I divider bits |
|---|---:|---:|---:|---:|---:|---:|
| 0.1 RPM | 4000 | 4194304005 | 32 | 655365000 | 30 | 32 |
| Binary Q7 | 51200 | 53687091264 | 36 | 8388672000 | 33 | 36 |
| Exact milliRPM | 400000 | 419430400500 | 39 | 65536500000 | 36 | 39 |

These are bounds for the **isolated bounded velocity profiles**, not permission
to truncate all callers in the active full position profile. Encoder rational
conversion and stopping-distance arithmetic have independent larger products.
Their widths need their own bounds. The 16- and 24-bit rows are reference points,
not sufficient for maximum-gain PI with the current contract.

A single divider sized to the larger P/I numerator can serve both operations;
separate P and I specializations can cause HLS to instantiate two units. The
standalone table measures each unit independently, excluding sharing muxes,
caller state machines, sign/absolute-value handling and rounding addition.
It therefore cannot establish complete-engine LUT savings. Integrating a bounded
shared PI divider and measuring the complete dispatcher is a subsequent test.

## Validation and reproduction

Each width is checked against native unsigned division using endpoint/carry
boundaries, powers of two, and 100000 deterministic random draws (zero denominators are skipped). Vitis C simulation
and RTL co-simulation use the same boundary cases plus 256 random draws to keep
simulation time bounded. Divide by zero is excluded by contract.

```sh
python3 tools/measure_dividers.py --output build/dividers_new --jobs 3
```

Use a fresh output directory. The script runs native checks, C simulation,
synthesis and Verilog co-simulation for each size, then writes `results.json`.
The original divider is synthesized separately at its original unsigned 64-bit
interface. These are HLS estimates, not post-route utilization or board validation.

## Tailored denominator sweep

The template also accepts a denominator width independently of the numerator.
Its remainder register and comparison/subtraction are denominator-sized, with an
explicit carry preserving the restoring-division invariant. Numerator/quotient
storage and the number of iterations remain numerator-sized. These are variable
divisors within the declared width, not constant-divisor substitutions.

| Candidate | P divider N/D bits | I divider N/D bits | Shared PI divider N/D bits |
|---|---:|---:|---:|
| 0.1 RPM | 32/4 | 30/14 | 32/14 |
| Q7 | 36/8 | 33/17 | 36/17 |
| Exact milliRPM | 39/10 | 36/20 | 39/20 |

Actual P divisors are 10, 128, and 1000; actual I divisors are 10000, 128000,
and 1000000 respectively. Shared sizes accommodate either divisor. Measuring
both separate and shared choices exposes the cost of duplicated arithmetic;
complete-engine arbitration and scheduling costs still require integration.

```sh
python3 tools/measure_dividers.py --output build/dividers_tailored_new --tailored --jobs 3
```

The Q7 proportional divisor is the constant 128, so a dedicated implementation
could replace that division with a shift after adding the rounding offset.
The measured 36/8 row intentionally tests a variable divisor, enabling a fair
width comparison; it is not the minimum possible constant-Q7 implementation.
Likewise, constant decimal divisors may be strength-reduced by synthesis. Those
are separate choices from this explicit restoring-divider sweep.

When integrating these unsigned dividers, the signed product must retain its
sign before taking a magnitude: the three P paths require signed product widths
of at least 33, 37 and 40 bits respectively. The unsigned widths in the tables
must not be applied directly to signed multiplication. Rounding additions are
included in the numerator bounds above; all casts must follow range validation.

## Measured resource tables

All 19 variants passed native checks, C simulation and Verilog co-simulation.
Every row uses **0 DSP and 0 BRAM18K**. FF means flip-flop registers.
Latency is deterministic; time is cycles / 20 MHz. These figures include each
standalone top and its control, not complete caller integration.

### Equal-width reference sweep

| Numerator / denominator bits | LUT | FF | Cycles | Time (us) | Estimated path (ns) |
|---|---:|---:|---:|---:|---:|
| 64 / 64 (original uint64_t) | 440 | 138 | 129 | 6.45 | 8.177 |
| 16 / 16 (ap_uint) | 232 | 56 | 33 | 1.65 | 6.058 |
| 24 / 24 (ap_uint) | 282 | 81 | 49 | 2.45 | 6.184 |
| 30 / 30 (ap_uint) | 318 | 99 | 61 | 3.05 | 6.437 |
| 32 / 32 (ap_uint) | 330 | 105 | 65 | 3.25 | 6.426 |
| 33 / 33 (ap_uint) | 336 | 109 | 67 | 3.35 | 6.575 |
| 36 / 36 (ap_uint) | 354 | 118 | 73 | 3.65 | 7.021 |
| 39 / 39 (ap_uint) | 372 | 127 | 79 | 3.95 | 7.466 |
| 48 / 48 (ap_uint) | 426 | 154 | 97 | 4.85 | 7.244 |
| 64 / 64 (ap_uint) | 522 | 202 | 129 | 6.45 | 8.177 |

### Tailored PI dividers

| Numerator / denominator bits | LUT | FF | Cycles | Time (us) | Estimated path (ns) |
|---|---:|---:|---:|---:|---:|
| 32 / 4 | 222 | 77 | 65 | 3.25 | 5.935 |
| 30 / 14 | 246 | 83 | 61 | 3.05 | 5.690 |
| 32 / 14 | 250 | 87 | 65 | 3.25 | 5.690 |
| 36 / 8 | 242 | 90 | 73 | 3.65 | 6.339 |
| 33 / 17 | 272 | 93 | 67 | 3.35 | 6.063 |
| 36 / 17 | 278 | 99 | 73 | 3.65 | 6.063 |
| 39 / 10 | 248 | 98 | 79 | 3.95 | 5.594 |
| 36 / 20 | 290 | 102 | 73 | 3.65 | 6.080 |
| 39 / 20 | 296 | 108 | 79 | 3.95 | 6.080 |

The 64-bit template uses more resources than the original uint64_t source
(522 versus 440 LUT; 202 versus 138 FF). Explicit arbitrary-width typing changes
HLS mapping even at the same nominal width, so use the original 440-LUT result
as the actual before/after baseline rather than claiming savings against 522.

| Format | Separate P + I LUT / FF (sum) | Shared LUT / FF | Shared LUT saving vs original 64-bit |
|---|---:|---:|---:|
| 0.1 RPM | 468 / 160 | 250 / 87 | 190 (43.2%) |
| Q7 | 514 / 183 | 278 / 99 | 162 (36.8%) |
| Exact milliRPM | 538 / 200 | 296 / 108 | 144 (32.7%) |

These separate-unit sums exclude connecting logic. Shared units serialize requests;
separate units can operate concurrently. A shared exact-milliRPM unit costs only
46 LUT and 21 FF more than the shared 0.1 RPM unit while preserving precision.
It is the preferred next integration experiment. Whole-dispatcher synthesis and
co-simulation must establish the net benefit after signed rounding, operand muxes,
and scheduling; do not subtract this standalone saving from previous dispatcher
reports as if it were a measured system result.

The [shared exact divider has now been integrated](velocity_exact_shared.md)
into the bounded velocity-only dispatcher: measured saving is 340 LUTs with
bit-exact retained behavior and passing RTL co-simulation.
