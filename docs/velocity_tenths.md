# 0.1 RPM arithmetic measurement

> Current product builds: [build guide](build.md). Commands below are specialized
> component/experiment reproduction or historical evidence, not the default build.


An isolated velocity-only candidate in `build/velocity_tenths` evaluates decimal
fixed-point PI inputs at ten units per RPM. The operating envelope remains
+/-200 gearbox-output RPM and a maximum 100 mm wheel diameter, at 20 MHz on
xc7z020clg400-1 with Vitis 2026.1. Main full-profile sources are unchanged.

The reference JGA25-370 uses 11 encoder pulses per channel per motor-shaft
revolution, X4 decoding, and a 46.8:1 gearbox: 44 motor-shaft counts and
10296/5 output-shaft counts per revolution. The estimator retains this exact
rational scaling and returns milliRPM. Only qualified PI inputs are rounded
here; this does not test a narrower reciprocal-period estimator.

## Arithmetic contract

- PI velocities: signed 12-bit, +/-2000 at the accepted speed limit.
- PI error: signed 13-bit, +/-4000 for opposing endpoint velocities.
- Round each velocity to nearest 0.1 RPM, ties away from zero.
- Integral: signed 31-bit, bounded by +/-65536 * 10000. At 1 kHz, accumulate
  `Ki * error` and convert to effort with denominator 10000; retain fractional
  accumulation, clamping and anti-windup.
- Keep the precise milliRPM ramp/remainder and public MMIO units. Even a
  1 milliRPM/second acceleration continues progressing below the PI quantum.
- Validate configuration and overspeed before narrowing; INT32 extremes cannot
  wrap into apparently valid feedback. Stop qualification uses original
  milliRPM feedback and tolerance, not rounded PI inputs.

Each PI input has at most 0.05 RPM rounding error; the difference can have up to
0.1 RPM error. This is not a bound on closed-loop tracking error. At the maximum
allowed Kp of 16, that difference can change unsaturated proportional effort by
up to 1.6 normalized duty, so coarse resolution can materially change behavior
with aggressive gains. Gain tuning, low-speed limit cycles and loaded tracking
still require physical validation. Arithmetic tests alone do not approve this
precision for deployment.

## Verification

The 14 applicable native/RTL tests pass under the existing UBSan build. The
motion test checks all 400001 accepted milliRPM inputs against an independent
rounding reference, signed PI output/integral accumulation across quantization
boundaries, endpoint acceptance, invalid configuration, out-of-range feedback,
sub-quantum ramp progress, saturation, reversal, controlled stop, timeout and
authorization loss. Existing tests exercise the independent protective and
encoder/AXI bridge RTL; they do not qualify a complete board integration.

## Reproduction

Use a fresh output directory:

```sh
python3 tools/prepare_bounded_velocity.py --variant tenths --output build/tenths_new
python3 build/tenths_new/tools/vitis_check.py --kernel motion
python3 build/tenths_new/tools/vitis_check.py --kernel dispatcher
```

The patch against the previous velocity-only experiment and measured evidence
are in `docs/evidence/velocity_tenths/`. All six changed candidate files were
reproduced byte-for-byte using the helper. Baselines are preserved in
`docs/evidence/velocity_only/` and `docs/evidence/velocity_bounded/`.

## Results

HLS resource estimates, not post-route utilization:

| Dispatcher variant | LUT | FF | DSP | BRAM18K | Max cycles |
|---|---:|---:|---:|---:|---:|
| Original velocity-only milliRPM | 21532 | 10695 | 22 | 4 | 801 |
| Bounded exact milliRPM | 21347 | 10635 | 20 | 4 | 801 |
| Previous binary Q7 | 21766 | 10706 | 22 | 4 | 804 |
| Decimal 0.1 RPM | 21628 | 10678 | 22 | 4 | 804 |

The tenths candidate adds 96 LUTs (0.45%) versus the original velocity-only
baseline and 281 LUTs (1.32%) versus bounded exact milliRPM. It saves 138 LUTs
versus Q7, but still does not justify reduced precision on resource grounds.
The smaller PI widths coexist with added unit conversions and the retained
milliRPM ramp/estimator; narrowing the entire measurement/control path would be
a different experiment.

The tenths motion kernel uses 11536 LUT, 5390 FF, 15 DSP and no BRAM, with a
489-cycle maximum (24.45 us), versus the previous velocity-only motion kernel's
11499 LUT, 5621 FF, 15 DSP and 489 cycles. Dispatcher maximum is 804 cycles
(40.20 us), with estimated critical path 34.617 ns, within the 36.50 ns effective
HLS constraint. Motion estimated path is 33.546 ns. Shared two-motor utilization
was not remeasured and no four-motor fit is claimed. Keep bounded exact milliRPM
as the preferred existing candidate; the active full profile remains unchanged.

Motion and dispatcher both passed Vitis C simulation, synthesis and Verilog
co-simulation. Their synthesis and co-simulation reports are preserved with the
native test log. Functional comparison uses the rounded arithmetic contract;
bit-exact equivalence to milliRPM PI output is intentionally not claimed.
