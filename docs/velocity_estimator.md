# C++ HLS encoder velocity estimator

`dmotor_velocity` in `src/velocity.cpp` estimates signed milli-RPM from a coherent
encoder-event snapshot once per 1 kHz control tick. The fabric rate is 100 MHz.
The computation is portable C++14; no vendor integer types or floating point are
used. This component is not yet wired into the complete AXI/motion controller.

## Measurement contract

`rtl/encoder_snapshot.sv` collects the existing decoder's accepted-event pulse,
direction, timestamp, and invalid-transition count. It maintains an unsigned
64-bit accepted-event counter, latest edge time, and a 32-bit direction epoch.
The direction epoch increments on every reversal, including a reversal and return
between estimator samples. Invalid transitions do not increment accepted count.
Decoder overflow and saturation of the invalid-transition counter latch a capture
fault. That fault must inhibit operation through the final safety integration.

Snapshot every field together with fabric time in the same clock domain. The
collector adds one clock of latency to registered decoder events; preserve the
original edge timestamp. The timestamp source and decoder timestamp must share
an origin. Reset the collector and estimator together on decoder initialization,
polarity/mode/scaling changes, and recovery. Do not reset only the estimator while
claiming previously captured edges are fresh measurements.

The estimator's first call establishes a baseline, not a valid velocity. Two
subsequently observed edge-bearing snapshots in an unchanged direction qualify
an estimate. Any direction-epoch or invalid-transition-count change discards the
spanning interval. A first edge-bearing snapshot after that change establishes
the new baseline; the next qualifies speed. Fault-free startup without edges
qualifies zero only when the configured timeout expires.

## Count/period algorithm

For consecutive usable snapshots, compute:

`milli_RPM = round(delta_events * 60 * FABRIC_HZ * 1000 * counts_den /
                  (delta_edge_time * counts_num))`

One new event gives a reciprocal-period estimate. Multiple events give an
edge-anchored count/time average of their periods. Both use the same formula, so
there is no discontinuous crossover requiring hysteresis. This avoids counting
edges against a fixed 1 ms window, which would quantize low speed severely.
The counts-per-revolution ratio is never rounded to an integer.

There is no additional low-pass filter in this initial profile. Averaging spans
one edge interval at low speed and the interval between the latest edges of
successive event-bearing control snapshots at higher speed. The `averaged` flag
means more than one interval contributed. Held estimates retain their last
averaging flag; it does not mean a new measurement occurred at this tick.

Direction supplies the sign. The magnitude rounds to nearest milli-RPM, ties
upward (equivalently away from zero once signed). Fractional nominal X4 CPR is
`10296/5`. With ideal constant-speed timing, first qualification occurs within
approximately two edge periods plus two control ticks after initialization:
about 60.3 ms at 1 RPM; high-speed snapshots qualify within a few ticks.
These are acquisition bounds, not closed-loop settling claims. An estimate
represents a past interval, with approximately half-interval averaging delay plus
the reported age and up to one control-tick observation delay. A motor model and
measured sensor jitter are still needed to assess dynamic phase lag.

## Timeout and validity

The proposed default zero timeout is **10,000,000 clocks (100 ms)**. Between edges,
report the held estimate and age of its newest contributing edge. After timeout,
report valid zero with a distinct timeout flag when age reaches the boundary.
A newly observed edge resets age before that check.
Age at startup is elapsed time since estimator initialization, because no usable
edge has been observed. A first edge after timed-out zero invalidates that zero
until another interval qualifies speed. A new invalid-transition diagnostic
restarts qualification and the no-edge timeout, rather than immediately asserting
zero from an older edge.

The estimator cannot distinguish a mechanically stopped motor from a disconnected
or stuck encoder solely from missing edges. Timeout-qualified zero is a measurement
status, not a diagnosis. Stall detection and fault escalation require additional
controller policy. Closed-loop activation must wait for qualified feedback;
passing invalid feedback to the existing motion executor intentionally faults.
The [combined controller](controller.md) now handles the first edge after timeout
and the first reversed edge using a bounded acquisition state. It keeps measurement
validity truthful while freezing control state and limiting held duty. Its generated
kernel still requires integration with the hardware queue and safety wrappers.


## Bounds and faults

Configuration is fixed until an explicit estimator reset. Accepted bounds:

- Nonzero configuration generation; numerator 1..1,000,000, denominator 1..10,000.
- Zero timeout 100,000..1,000,000,000 clocks (1 ms..10 s).
- Exactly 100,000 fabric clocks between snapshots, at most 100 new accepted events.
- Newest accepted edge is at most one control tick old; latest timestamp cannot
  change without a changed accepted-event count.
- Qualified interval at least 1,000 clocks per event on average and at most 10 s;
  output magnitude at most 1,000,000 milli-RPM.

The collector/decoder must enforce the architecture's per-transition capture
spacing; this snapshot-level average check cannot prove every individual spacing.
Unsigned counter/time differences support wrap within the bounded observation
intervals. At most 100 events per tick also makes a direction-epoch wrap impossible
within one snapshot interval. The narrower denominator bound than `MotionConfig`
is deliberate in this first numerical profile and must be enforced by the future
shared configuration commit path. Nominal `10296/5` is well within it.

The largest numerator is `100 * 6e12 * 10000 = 6e18`; the largest denominator is
`1e9 * 1e6 = 1e15`. Addition of half the denominator remains within unsigned
64-bit range. Speed is range checked before converting to signed 32 bits.

Fault masks are `1` invalid configuration, `2` changed configuration or incorrect
tick spacing, `4` incoherent/out-of-envelope snapshot or saturated invalid counter,
and `8` out-of-range speed. Faults are sticky until reset and invalidate output.
They must feed shared HALT through the future controller integration; the estimator
itself does not drive electrical pins or acknowledge safety faults.

## Internal HLS interface and tests

The scalar top accepts reset, current fabric time, the five snapshot fields,
configuration generation, rational CPR, and zero timeout. It returns signed
milli-RPM, 64-bit age in clocks, and status: valid bit 0, timeout-qualified zero
bit 1, multi-interval estimate bit 2, and fault mask starting at bit 8. This is an
internal interface; no new public MMIO registers are claimed.

`velocity_test` drives the same top used for Vitis C/RTL co-simulation. It sweeps
ideal 1, 5, 30 and 130 RPM in both directions over 400 ms, assessing every 1 ms
sample from 100 through 400 ms against the proposed 2% error bound. It also tests
zero timeout at the boundary, resumed edges, reversal and reversal-return,
invalid-transition recovery, changed configuration, missed ticks, excessive event
counts and a future edge timestamp. The separate RTL collector test covers
counting, hidden reversals, invalid exclusion and sticky capture faults.

Run `python3 tools/vitis_check.py --kernel velocity` for the 100 MHz
`xc7z020clg400-1` flow. See [verification evidence](verification.md). Combined
encoder-to-estimator generated-RTL wiring, complete hardware sequencing, motor
model dynamics, tuning MMIO and hardware measurements remain integration work.

## Bounded conversion operands

After the full-width snapshot and configuration checks, accepted event delta is
at most 100, CPR denominator at most 10,000, and numerator at most 1,000,000.
The exact numerator now multiplies event delta by the CPR denominator first
(product at most 1,000,000), then widens before multiplying by the clock/RPM scale.
The measured interval is narrowed to 32 bits only after the maximum-interval check;
`timing.hpp` already asserts that this interval bound fits in `uint32_t`.
The CPR numerator uses its proven 20-bit width for the denominator product.

These operations preserve the original integer rational expression, rounding,
signed speed and invalid-input rejection. They expose existing bounds to synthesis
rather than changing encoder precision or configured limits.
