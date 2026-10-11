# C++ HLS motion executor

`src/motion.cpp` implements the portable `MotionExecutor` and Vitis top
`dmotor_motion`. It executes one control boundary per call at **1 kHz**, with
**100 MHz** fabric timestamps. Calls come from hardware, not processor software.
This is an independently verified component, not the complete motor controller.

## Commands and completion

The executor consumes the existing 16-word descriptor. It revalidates the
session, configuration generation, operand bounds, and relative-position
addition at activation. Invalid descriptors produce rejected completions.

- Velocity installs the target and completes immediately; PI regulation continues.
- Absolute/relative position selects a speed-limited profile, decelerates near
  the target, and completes after the configured position/velocity settling window.
  Relative targets resolve from measured position at activation.
- Open-loop installs bounded signed Q16 duty and completes immediately.
- Dwell preserves the installed drive mode until its duration expires.
- Queued controlled stop ramps toward zero and completes after velocity settling.

Completion remains valid until acknowledged. This blocks another activation,
but does not stop regulation. An acknowledgment can coincide with the next
activation; the dispatcher must remove an accepted descriptor to avoid replay.
BUSY describes an unfinished command, not continuing velocity regulation.

Immediate controlled stop preempts the active command with an aborted completion
(detail `0x200`), prohibits new activation during stopping, and ramps from measured
speed. Its request must be a **single sampled event**, not a persistent stop level.
The queue dispatcher must also cancel queued work according to the session contract.
Authorization loss zeroes duty and aborts active work (detail `0x100`). Hardware
inhibition bypasses these scheduled calculations; ring cancellation is authoritative
and prevents duplicate retirement of the executor's now-stale completion.

Position completion and controlled-stop completion select zero drive/coast,
with no holding loop. Timeout wins over settling at the same boundary.
Position tolerances and gains remain vehicle-calibration inputs; test values
are synthetic and are not application acceptance criteria.

## Numeric behavior

Speed uses signed milli-RPM. Acceleration/deceleration use milli-RPM/s; fractional
increments accumulate between 1 ms ticks. Reversal decelerates the target ramp
to zero before ramping in the opposite direction. Closed-loop takeover from
coast/open-loop starts at measured speed.

The initial position profile compares integer position error with the ceiling of
`ramp_mrpm² * counts_num / (120000 * deceleration_mrpm_s * counts_den)`
and adds one tick of distance lookahead. The counts-per-revolution ratio remains
rational; it is never rounded to an integer CPR. This is a provisional braking
heuristic using reference speed, not a guarantee about actual vehicle stopping.
Closed-loop plant simulation and physical overshoot/settling tests remain required.

PI uses `P = round(kp_q16 * error_mrpm / 1000)` and an integral accumulator in
billionths of a Q16 duty LSB. Each tick adds `ki_q16 * error_mrpm * 1000` to that
accumulator. The integral is bounded and committed only when unsaturated or
when the error drives output back out of saturation. Output rounding is nearest,
ties away from zero. Fractional increments survive across ticks. This executor
contains its own PI calculation; do not instantiate the older `dmotor_pi` in series.

Accepted bounds: max speed 1,000..1,000,000 milli-RPM; acceleration/deceleration
1..10,000,000 milli-RPM/s; rational CPR numerator/denominator each 1..1,000,000;
`max_speed_mrpm * counts_num <= 6,000,000,000 * counts_den`; settling ticks
1..1,000,000; nonnegative Q16 gains up to 1,048,576; duty/integral limits
0..65,536; velocity tolerance at most max speed; nonzero stop timeout.
Position tolerance is unsigned 32-bit counts. Configuration generation and session
must be nonzero. These bounds keep products and ceiling additions within 64 bits:
profile numerator at most 1e18, denominator at most 1.2e18, PI increment below
2.098e15, and integral magnitude at most 6.554e13. Position difference uses unsigned
64-bit subtraction to cover the full signed position span without signed overflow.

## Internal HLS interface

These are internal arrays, **not new public MMIO addresses**. The tuning/configuration
commit bridge remains to be implemented. Configuration is stable throughout the
enabled session. Any same-generation mutation violates the caller contract.

| Configuration word | Value |
|---|---|
| 0 | Generation |
| 1 | Maximum speed, signed milli-RPM |
| 2 / 3 | Acceleration / deceleration, milli-RPM/s |
| 4 / 5 | Encoder counts-per-revolution numerator / denominator |
| 6 | Position tolerance, counts |
| 7 | Velocity tolerance, milli-RPM |
| 8 | Consecutive settling ticks |
| 9 / 10 | Proportional / integral gain, Q16 |
| 11 / 12 | Duty / integral limit, Q16 |
| 13 | Immediate-stop timeout, microseconds |
| 14 / 15 | Reserved, must be zero |

| Reply word | Value |
|---|---|
| 0 / 1 | Signed Q16 duty / signed ramp milli-RPM |
| 2 | Flags: ready bit 0, busy bit 1, completion valid bit 2, accepted bit 3 |
| 3 / 4 | Completion sequence / epoch |
| 5 / 6 | Completion outcome / detail |
| 7 | Sticky executor fault bits |
| 8 / 9 | Activation timestamp, low / high |
| 10 / 11 | Terminal timestamp, low / high |
| 12..15 | Zero |

Fault masks: `1` invalid configuration/session, `2` skipped tick or changed
session/generation while authorized, `4` movement/stop timeout, `8` missing or
out-of-range velocity feedback during closed-loop control. Faults inhibit drive,
abort active work, and remain latched until reset. Invalid feedback is checked
before an immediate velocity command can report success. All must feed shared
HALT through the final safety integration. No encoder velocity estimator is
implemented by this kernel; `velocity_valid` denotes a qualified external estimate.

## Scheduler/PWM adapter

`rtl/motion_top.sv` snapshots the internal command/configuration/feedback mailbox,
runs the generated Vitis core under `dmotor_scheduler`, and applies retired duty
to `dmotor_pwm`. Policy remains C++; RTL handles synchronous memory ports,
handshakes, snapshotting, deadline protection and immediate inhibition.

Mailbox inputs must remain stable until `inputs_sampled`. Keep commands pending
until a retired result reports acceptance; acknowledge completions only after
successful ring retirement. Stop and completion acknowledgment are single sampled
events. Results become visible together on `result_valid`. The timestamp must
advance once per 100 MHz clock, even during inhibition. The scheduler enforces
its independent 800 us deadline and maintains last/maximum latency measurements.

The adapter's `inhibit` must include reset/recovery authorization, watchdog, HALT,
and other protection causes. Local executor faults and scheduler deadlines also
inhibit PWM. External safety must latch/escalate these causes and cancel ring work.
Inhibition resets/discards in-flight executor state; reauthorization alone must
never replay an old mailbox. This adapter does not implement that lifecycle policy,
AXI dispatch, the encoder estimator, or the DRV8263H electrical pin mapping.

## Verification

`motion_test` exercises all six commands, retained completions, reversal, position
settling, timeout priority, fractional ramp/integral increments, invalid feedback,
closed-loop takeover, authorization loss, skipped ticks and relative overflow.
The same testbench runs in Vitis C simulation and RTL co-simulation.
`motion_queue_test` connects native ring and executor classes to check ordered
execution, dwell, queued stop, completion backpressure and cancellation; it is
not a synthesized AXI-to-motion dispatcher.

`tools/motion_rtl.py` checks the actual generated HLS core with the scheduler/PWM:
autonomous launches, completion acknowledgment, 50% PWM over a full 5,000-clock
frame, inhibition during an HLS transaction, and a latched feedback fault.
Regenerate with `python3 tools/vitis_check.py --kernel motion` before running it;
CTest does not regenerate HLS RTL. See [verification results](verification.md).

The newer [combined controller](controller.md) uses an internal bounded-acquisition
mode while speed is being qualified. The standalone `dmotor_motion` top remains
strict: invalid closed-loop feedback faults. Only the combined coordinator owns
the hold deadline, duty cap and protective fault arbitration.


## Bounded fractional ramp arithmetic

The retained ramp fraction is below 1000. With the validated acceleration or
deceleration rate at most 10000000 milli-RPM/s, their sum is at most 10000999,
which fits in 24 bits. The implementation uses portable `uint32_t` storage and
addition instead of the former 64-bit accumulator. Division and modulo by 1000
retain exactly the original fractional behavior, with no approximation or new
state. Reversal, clamping, takeover, reset and authorization loss keep the same
fraction/reset rules.

The motion testbench compares the HLS top against the original wide-arithmetic
ramp over rates 1, 999, 1000, 1001, 9999999 and 10000000 milli-RPM/s, including
rate changes, reversal, target clamping and reset. Position-profile divisions,
PI arithmetic and per-tick full configuration validation remain unchanged.

An experimental cache of whole/remainder components for both rates increased
area and was discarded. See `verification.md` for the measured comparison.

## Exact arithmetic reuse

The implementation now isolates unsigned division and signed nearest rounding in
`dmotor::detail` functions. Vitis configurations preserve these function boundaries
and share their call sites within the motion step. Signed rounding computes the
magnitude once, divides once, then restores the sign; ties still round away from
zero. Position braking/lookahead retain ceiling division. Elapsed time retains
floor division. No reciprocal approximation or reduced numeric range is introduced.

All denominators are positive after existing configuration/command validation;
existing numeric bounds also prevent overflow in rounding/ceiling additions.
Full configuration validation remains in the authorized tick path. This does not
introduce a configuration cache or trust software-derived coefficients.

Reproduce the native baseline comparison with:

```sh
python3 tools/check_motion_equivalence.py --reference d4abc0b
```

This compares every reply word for 16,384 deterministic motion updates, including
PI gain extremes, signed targets/feedback, all six command types and timeouts.
It uses the current protocol validator for both versions and checks motion-source
equivalence only; it is not a proof for every input or a hardware timing test.
