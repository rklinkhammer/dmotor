# PS motion executor

`src/motion.cpp` implements the fixed-point `MotionExecutor` used by the four
owned PS controller contexts. It advances at **1 kHz**, using **20 MHz fabric
timestamps** captured by the PL scheduler. The C++ call executes in the daemon's
PS worker through the ticketed [PSF1 transaction](ps_executor.md).

## Product behavior

The product accepts velocity and immediate controlled STOP operations. Position,
relative-position, dwell and queue-clear opcodes are rejected, as in the previous
prepared 400 RPM velocity product. Higher-level position/trajectory planning
belongs on the PS above `dmotor_client`.

Velocity targets use signed milliRPM with a commissioned maximum of 400 RPM.
Fractional acceleration/deceleration retains its remainder across 1 ms ticks;
fixed-point PI, anti-windup, configured duty limits, STOP settling and bounded
STOP timeout retain the original product arithmetic. Encoder estimation and
acquisition are handled by the surrounding Controller/VelocityMailbox context.
Configuration validation and per-context initialization precede normal execution.

The worker operates on a speculative context copy. Only hardware acceptance of
the matching response ticket commits that copy. Cancelled work does not advance
state. Hardware applies complete four-wheel results coherently; independent
PL deadlines, HALT, encoder capture and PWM continue when PS execution stalls.

## Verification and scope

The native motion/controller/shared-mailbox tests exercise the direct product
sources. The PS reference tests compare complete replies from independent motor
contexts; fused-operation tests compare staging, commit and tick against the
serial operation sequence. Full-controller co-simulation exercises the actual
PS C++ engine with atomic mailbox HLS and the surrounding RTL.

Use [the product build](build.md); the old standalone motion HLS flow is not a
product dependency. Earlier position-capable component interfaces, arithmetic
measurements and HLS experiments remain in the
[historical component reference](archive/pre-ps-executor-2026-11-07/motion_executor.md).
See [architecture](../DMOTOR_ARCH.md), [verification](verification.md) and
[current build/qualification status](system_status.md). Hardware timing and
physical motor behavior remain unqualified pending assembled boards.
