# Development contract and implementation status

This is the first infrastructure implementation against architecture revision 0.16.
The release ABI is not frozen. This workspace is not a deployable motor controller.

## Partition and reuse

Inspected the local dvrmc-portable control arithmetic and coding standards and the
original dvrmc Vivado project layout. The original project is not a Git checkout;
implementation history was not available locally. No reference files were modified.
The new code follows its fixed-width arithmetic, bounds, anti-windup, and formatting
conventions. Its two-motor software transaction interface and integer/fixed-point
gear scaling were not copied. The new protocol reference uses the architecture's
single-motor descriptor and completion records. No claim of reuse equivalence is made.

## Implemented components

- `CommandRing`: common native/HLS ring state with `dmotor::ring_depth` immutable published
  slots, completion reservation, wrap-safe counters, descriptor validation,
  cancellation ranges, and explicit drained-session reopening. The `dmotor_command_engine` HLS top now synthesizes this storage and behavior
  and adds hardware polling, completion tokens, and cancellation priority. It does
  not implement AXI handshakes or prove an upstream bus has drained. See
  [the internal command-engine contract](command_engine.md).
- `dmotor_safety`: hardware watchdog, two-stage external HALT synchronization,
  sticky local/external causes, explicit recovery, shared-HALT request, and
  timestamps. Every local cause requests shared HALT. Inputs other than HALT must
  already be synchronized and qualified. Timeout and session must remain stable
  while enabled. The future MMIO layer must validate configuration and fresh
  session prerequisites before asserting enable. Reset must be synchronously
  released; physical output protection before configuration is integration work.
- `dmotor_halt_aggregator`: external latched shared stop, no automatic release.
- `dmotor_scheduler`: periodic HLS launch, held start/ready handshake, bounded
  deadline, late-result rejection, latency measurements, and engine reset on
  inhibition. Deadline fault is sticky until scheduler reset by the recovery path.
- `qcencoder`: independent synchronized X1/X2/X4 decoder, invalid-transition
  rejection, saturating signed 64-bit position and error counter, event timestamp.
  Configuration is supplied stable; the module does not implement the register
  configuration bridge or fault thresholds.
- `dmotor_pwm`: boundary updates, signed duty clamp, one full frame of reversal
  blanking, immediate combinational logical inhibit, and latched direction.
  This is not the DRV8263H pin adapter. PWM period is an elaboration parameter in
  this prototype; runtime period configuration remains work.
- `dmotor_pi`: stateless vendor-independent bounded PI arithmetic with explicit
  previous integral input and next integral output. HLS handshake ports are internal
  to the future autonomous scheduler adapter, never CPU tick commands.

- `dmotor_motion`: C++ HLS command execution, position profiles, fractional velocity
  ramps, PI with retained integral residuals, dwell, settling, timeouts, and
  controlled stop. `dmotor_motion_top` connects the generated core to the existing
  autonomous scheduler and logical PWM stage. See [interface and limits](motion_executor.md).
  Queue-to-motion hardware dispatch, estimator wiring and tuning commit remain pending.

- `dmotor_velocity`: rational edge-anchored count/period estimation, sample age,
  zero timeout and reversal/invalid-transition qualification in C++ HLS.
  `dmotor_encoder_snapshot` retains event counts and direction epochs between ticks.
  See [the measurement contract](velocity_estimator.md). C++ estimator-to-motion sequencing is now implemented by `dmotor_controller`;
  complete hardware wiring and dynamic motor-model validation remain pending.

- `dmotor_controller`: combined estimator/motion tick with bounded acquisition,
  limited held duty, deferred activation and sticky protection causes. See
  [the coordinator contract](controller.md). Full bus/queue/safety/PWM wiring remains work.

- `dmotor_dispatcher`: single-owner ring/controller service with current-tick offer/
  accept/capture, exact completion timestamps and cancellation priority. See
  [the dispatcher contract](dispatcher.md). It is not yet connected to AXI/PWM.

## Provisional numerical kernel

The PI kernel accepts target/measurement within +/-1,000,000 milli-RPM, tick
period 1..100,000 us, nonnegative Q16 gains up to 16, and integral/duty limits
0..65536. With error `e` in milli-RPM:

- `P = round(kp_q16 * e / 1000)`.
- `I_candidate = clamp(I + round(ki_q16 * e * dt_us / 1e9), +/-I_limit)`.
- Commit I only when candidate duty is unsaturated or error drives it out of
  saturation. Output is clamped `P + I`.

Rounding is nearest with signed ties away from zero. The largest intermediate
magnitude is below 2.098e17, inside signed 64 bits. Invalid input produces zero
outputs and a range-fault flag. There is no derivative or feedforward yet.
This primitive rounds the integral increment each call: fractional integral
residual retention is implemented in the newer motion executor, which includes
its own PI arithmetic and does not instantiate this primitive. It is not a tuned or complete motion-control algorithm.

## Draft MMIO

`abi/registers.json` is the single source for the initial register subset.
`tools/generate_abi.py` generates C++ constants, SystemVerilog constants and a
review table. Missing telemetry/configuration/fault/interrupt fields remain
explicit follow-on work. Ring registers now have HLS behavior and an AXI bridge; unsupported writes fail.
See [the implemented subset and transport contract](axi_peripheral.md).
Raw descriptor/completion words have explicit little-endian wire encoding; native
objects must not be memcpy'd to a device on an assumed host endianness.

## Required next integration gate

The AXI-to-HLS ring path is now implemented and tested with generated Vitis RTL.
The staged timeout/speed configuration and safety-authorization bridge are now
connected. The motion executor and its generated-RTL scheduler/PWM path are now
implemented separately. The new HLS dispatcher owns queue/controller acceptance,
completion tokens and cancellation. Next adapt AXI to that owner, arbitrate hardware
ticks, connect encoder snapshots and PWM retirement, and add tuning commit,
coherent telemetry and interrupts. Route dispatcher faults to the shared-HALT safety
path and test cancellation/recovery across the complete hardware wrapper.
The older native ring/executor test remains a reference; the new dispatcher has
its own Vitis synthesis and RTL co-simulation evidence.

The HLS command engine and stand-alone RTL modules do not yet pass V01–V08 end to end.
Both vendor flows must validate the integrated infrastructure before expanding
features, as required by the architecture's implementation phases. SmartHLS is
not available in the inspected installation. Hardware integration, CDC review,
placed/routed timing, and vehicle acceptance remain pending.
