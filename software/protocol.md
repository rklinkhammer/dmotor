# Typed controller protocol

`dmotor_sw::Protocol` is the first hardware-facing software layer. It uses a
`Device` interface and the existing generated register definitions and controller
host helpers. An opt-in inhibited [Linux mapping backend](linux/README.md) is available; the
production backend is still pending. The protocol never enables a
motor, acknowledges a fault or resets the controller automatically.

## Configuration

`MotorConfiguration` names all 17 meaningful configuration fields; encoding emits
all 20 words, with the three reserved words zero. Units are part of field names:
milliRPM, milliRPM/second, Q16, microseconds and 20 MHz fabric cycles. Encoder CPR
retains numerator/denominator precision (the reference motor uses 10296/5).
Position tolerance remains in the wire layout for compatibility; this does not
restore FPGA position commands to the velocity profile.

Validation checks generation, the software profile's 1–400 RPM maximum-speed
range, acceleration/deceleration, CPR and event-rate bounds, gains, duty limits,
settling and acquisition/zero-timeout relationships. It is conservative admission
validation, not evidence that gains or electrical limits suit the physical motor.
HLS numeric validation and readiness remain authoritative after commit.

`configure()` rejects enabled controllers and invalid input before staging. The
caller must already hold core reset; the current ABI has no readable core-reset
bit, and the software does not infer reset from the broader blocked status. A
backend must surface the hardware's rejection if that precondition is violated.
No generation advancement or lifecycle orchestration is implemented here.

## Inspection

`compatible_controller()` reads DMI1 at 0x1000 only, allowing local inspection when
the mailbox is blocked by HALT/reset. `compatible_mailbox()` checks DMB1, schema
and motor count separately; call it only when atomic accesses are available.

`status()` preserves the raw word and decodes enabled/configured/ready/stopped,
fault, shared HALT, blocked and target-age masks. It is a live read, not part of
the captured telemetry bank. Configured is not equivalent to ready.

`capture()` performs one bounded coherent-bank read and decodes all 21 words per
motor. It preserves signed velocity/duty/position, 64-bit ticks/ages/fault times,
validity/acquisition/stopping flags, raw fault/rejection bits and engine sequences.
The engine sequences are **not global atomic-batch sequences**. Global application
tracking must use the existing atomic-mailbox applied-batch interface.

Snapshot coherence means the bank did not change while software read it. It does
not mean every field was sampled at the same physical time: engine telemetry
carries its frame tick, whereas encoder position/protection are copied at the
snapshot request. Do not assign the engine sample tick to the position observation
without a stronger hardware timestamp contract. The model cannot validate this
hardware relationship.

The adapter's error branch copies the engine reply without substituting requested
velocity into word 0. Therefore `requested_mrpm` is meaningful for normal successful
frames, and must not be treated as a reliable requested-target observation after
an adapter/engine fault. Fault-aware runtime handling and any required ABI change
remain follow-up work. Missing diagnostics are not synthesized.

## Backend and failure contract

`Device` requires serialized aligned 32-bit peripheral-relative accesses and a
platform device-completion fence. Reads/writes/fences throw `IoError` on transport
failure. The Linux backend must supply device-memory mapping, ownership and actual
ordering; a C++ compiler fence is insufficient. The test fake performs ordinary
in-process operations and is not an MMIO implementation.

A changed capture sequence returns false; an I/O failure throws. Both leave the
caller's snapshot unchanged. Configuration failures may leave partial staging, or
an effective commit if its final fence failed. Do not blindly retry, enable or
report configuration success after an exception. Lifecycle reconciliation belongs
to the future sole-owner service.

## Validation and next work

`dmotor-protocol-tests` runs alongside the semantic model tests natively and in
EDF's `do_semantics` ARM QEMU task. It verifies wire layout, status/telemetry
decoding, signed extremes, rejection and injected failures at every access/fence.
These tests do not establish Linux mapping, AXI error delivery or fence semantics.

Next is the inhibited Linux backend and OS-enforced sole owner, followed by
service/client IPC and maintenance lifecycle. Shared-HALT GPIO binding and
acknowledgment must be qualified before any maintenance motion is enabled.
