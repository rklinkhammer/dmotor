# Integrated shared exact PI divider

> Current product builds: [build guide](build.md). Commands below are specialized
> component/experiment reproduction or historical evidence, not the default build.


The bounded exact-milliRPM velocity-only candidate now uses one 39-bit numerator /
20-bit denominator divider for all proportional and integral rounding operations.
It lives in `build/velocity_exact_shared`; the active full position/motion profile
is unchanged. This integrates the previously measured arithmetic unit into the
complete candidate dispatcher, not into the two-motor shared-engine experiment.

## Implementation and range contract

`detail::divide_pi` uses an explicit 39-bit combined dividend/quotient register
and 20-bit remainder/divisor under Vitis. The native model applies the same masks
at each step. Both execute 78 loop phases, two per numerator bit, with the shifted
remainder carry preserved. Public C++ arguments retain ordinary integer types;
the arithmetic storage is explicitly sized with `ap_uint` for synthesis and
Vitis simulation (`DMOTOR_USE_AP_INT`). This is not just narrowing a value and
then calling the old 64-bit divider.

The signed `rounded()` wrapper computes magnitude and applies ties-away-from-zero
rounding before calling the bounded unit. At +/-200 RPM, error <=400000 milliRPM
and gain <=1048576, the maximum P numerator including rounding is 419430400500.
The clamped integral numerator including rounding is <=65536500000. Divisors are
1000 and 1000000. These fit 39 and 20 unsigned bits respectively. Configuration,
feedback and command validation establish the bounds before the PI path runs.
The helper is not safe for arbitrary full-range division; its header states the
preconditions. Encoder rational conversion retains the general 64-bit divider.

Existing directives share `rounded()` across the three PI rounding calls. The new
divider has a no-inline directive, leaving one identifiable shared core. Generated
RTL confirms exactly one `rounded` instance in `step_impl`, one `divide_pi` inside
`rounded`, and a separate `divide` in the estimator. Native and HLS models preserve
public milliRPM units, fractional ramp/integral accumulation, saturation,
anti-windup, stop behavior, and exact 10296/5 encoder scaling.

## Complete dispatcher measurements

Vitis 2026.1, xc7z020clg400-1, 20 MHz, same constraints as the bounded exact baseline.
These are HLS estimates, not post-route results.

| Metric | Bounded exact baseline | Integrated shared divider | Change |
|---|---:|---:|---:|
| LUT | 21347 | 21007 | -340 (-1.59%) |
| FF | 10635 | 10443 | -192 |
| DSP | 20 | 20 | 0 |
| BRAM18K | 4 | 4 | 0 |
| Maximum latency, cycles | 801 | 651 | -150 |
| Maximum latency at 20 MHz | 40.05 us | 32.55 us | -7.50 us |
| Estimated critical path | 34.617 ns | 34.617 ns | unchanged |

The integrated `divide_pi` instance is estimated at 225 LUT, 70 FF, 0 DSP/BRAM and
80 cycles. This differs from the isolated 296-LUT standalone wrapper because
caller context, port handling and synthesis specialization affect mapping; the
complete-dispatcher before/after values above are the meaningful integration result.
The full motion kernel synthesizes to 10978 LUT, 5322 FF and 13 DSP. Do not compare
that against a full position-profile motion kernel as if the divider were the
only difference. A matching bounded exact dispatcher baseline is preserved.

## Verification and reproduction

Fourteen applicable native/RTL tests pass with the existing UBSan build. The
arithmetic test adds full 39/20 bounds, maximum P/I rounded numerators, actual
PI divisors and 100000 random nonzero-denominator pairs. The same test passes
compiled with Vitis `ap_uint` types, exercising identical width truncation.

The retained-behavior trace matches all 4451 motion, 990 controller and 1251
dispatcher reply records against the previous full implementation within the
retained profile. This includes all reply words, not just final duty. The
14-test subset does not qualify the removed position/dwell/raw-duty commands
or the complete board integration.

```sh
python3 tools/prepare_bounded_velocity.py --variant exact-shared --output build/exact_shared_new
python3 build/exact_shared_new/tools/vitis_check.py --kernel motion
python3 build/exact_shared_new/tools/vitis_check.py --kernel dispatcher
python3 tools/check_bounded_velocity.py --profile build/exact_shared_new \
    --reference-tests build/velocity_only/tests
```

Use fresh output directories. Prepare `build/velocity_only` with
`tools/prepare_velocity_profile.py` first if it is absent. The integration patch,
reports, trace equivalence and native logs are under
`docs/evidence/velocity_exact_shared/`. The preparation helper reproduced all
seven changed integration files byte-for-byte. Whole-profile adoption and
shared two-motor remeasurement remain separate from this candidate integration.

Motion and complete dispatcher passed Vitis C simulation, synthesis and Verilog
co-simulation with the explicitly sized arithmetic enabled. `hierarchy.txt`
records the generated instance checks establishing sharing.

A subsequent [one-phase-per-bit evaluation](velocity_exact_onephase.md) reduces
complete-dispatcher usage further to 20935 LUT and 534 cycles, with exact behavior.
