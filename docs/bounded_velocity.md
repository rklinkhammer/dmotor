# Bounded velocity implementation and measurement

> Current product builds: [build guide](build.md). Commands below are specialized
> component/experiment reproduction or historical evidence, not the default build.


These are isolated velocity-only profiles, based on the existing experiment at
revision 17116b7, at 20 MHz on xc7z020clg400-1 with Vitis 2026.1. The active full
position/motion profile is unchanged. Neither candidate is a deployed ABI.

## Operating model

Named constants in each candidate's `include/dmotor/operating_limits.hpp` set the
maximum shaft speed to 200 RPM and maximum wheel diameter to 100 mm. The diameter
is host-side geometry metadata; this experiment adds no wheel-diameter register or
FPGA linear-distance conversion. Encoder counts per revolution must describe the
wheel/output shaft, including gearing. At 130 RPM a 100 mm wheel gives 0.681 m/s;
at 200 RPM it gives 1.047 m/s. Wheel slip and effective loaded diameter remain
system-controller concerns.

Configuration above 200,000 milliRPM is rejected. Estimator overspeed and motion
feedback are checked in the original units before narrowing. The rational
estimator still returns integer milliRPM; its division remains wide. Encoder
counts, timestamps and reporting remain 64-bit. MMIO words retain their layout
and units. Independent HALT/watchdog/protection paths are unchanged.

## Two implementations

`q7` rounds qualified ramp and measured velocity separately to signed 16-bit
values with seven fractional bits (1/128 RPM), before a signed 17-bit PI error.
Each conversion differs by at most 1/256 RPM; the error difference can reach
1/128 RPM. This is input quantization, not a bound on closed-loop tracking error.
At the maximum Kp of 16, that error difference can change proportional effort by
up to 0.125 duty, before output rounding/saturation. Hardware tuning is still
required. The milliRPM ramp and fractional remainder are retained so very slow
acceleration continues to advance; the entire ramp/estimator is NOT 16-bit.

`exact` retains milliRPM precision with a 19-bit velocity envelope and a 20-bit
PI error. It avoids the added conversion arithmetic. Target narrowing follows
command/configuration validation. Bounds do not imply every stored C++ integer
or MMIO word has the narrow type; explicit sign-extension operations expose the
validated arithmetic widths to HLS, following this project's existing convention.

Both cancel the common control-period factor in integral accumulation. At the
fixed 1 kHz rate, add `Ki * error` each tick and divide by `units_per_RPM * 1000`
when producing effort. This is algebraically exact for each candidate's units,
including clamping, rounding and anti-windup. Q7 requires 34 integral bits;
milliRPM requires 37, versus the original 47. A static assertion requires the
width to be re-derived if control rate changes. Existing `integral_nano_` is a
legacy private member name; its candidate scaling is described in the header.

## Measurements

HLS estimates, not post-route utilization. Compare the same single-motor
velocity-only dispatcher including its four-entry ring:

| Profile | LUT | FF | DSP | BRAM18K | Maximum cycles |
|---|---:|---:|---:|---:|---:|
| Previous velocity-only | 21,532 | 10,695 | 22 | 4 | 801 |
| Q7 PI inputs | 21,766 | 10,706 | 22 | 4 | 804 |
| Exact bounded milliRPM | 21,347 | 10,635 | 20 | 4 | 801 |

Q7 adds 234 LUT (1.09%); exact saves 185 LUT (0.86%) and two DSPs. The Q7 motion
kernel alone increases from 11,499 to 11,687 LUT. Its smaller arithmetic does not
offset conversion and control costs. Q7 is retained as evidence, not recommended
for adoption. Exact is the default reproducible candidate. Neither result proves
that four motors fit; the shared two-motor engine has not been remeasured here.

## Verification and reproduction

Both candidates pass 14 applicable native/RTL tests under the existing UBSan
build, including motion, velocity, controller, dispatcher and the independent
protection/encoder/AXI bridge tests. The motion bench exhausts every input from
-200,000 through +200,000 milliRPM, compares PI output against an independent
arithmetic reference, checks both endpoints, invalid configuration, INT32 extreme
feedback, signed ramps, anti-windup, stop, timeout and authorization loss. Q7
motion/velocity/dispatcher C simulation, synthesis and Verilog co-simulation pass.
Exact dispatcher C simulation, synthesis and Verilog co-simulation are recorded
alongside them. These tests do not qualify a complete board integration.

The exact candidate also matches 4,451 motion, 990 controller and 1,251 dispatcher
reply records from the previous retained-behavior trace, including every reply
word. Q7 intentionally changes arithmetic, so bit-exact equivalence is not claimed.

Use fresh output directories:

```sh
python3 tools/prepare_bounded_velocity.py --variant exact --output build/bounded_new
python3 build/bounded_new/tools/vitis_check.py --kernel dispatcher
# To reproduce the fixed-point experiment, select --variant q7 instead.
```

Patches and reports live in `docs/evidence/velocity_bounded/`. To repeat the exact
comparison, first prepare the previous velocity-only profile, then run:

```sh
python3 tools/prepare_velocity_profile.py --output build/reference_velocity
python3 tools/check_bounded_velocity.py --profile build/bounded_new \
  --reference-tests build/reference_velocity/tests
```

Adoption still requires choosing the profile/capability contract and integrating
it with the host and hardware wrapper. This experiment preserves the main full
profile rather than silently dropping position commands.

The subsequent [0.1 RPM experiment](velocity_tenths.md) also failed to reduce
LUT usage below exact bounded milliRPM; its smaller PI values alone do not
justify coarser precision.
