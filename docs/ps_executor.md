# PS motor executor — PSF1, 1 ms

Main uses four owned `VelocityMailbox` contexts in
`software/include/dmotor_sw/ps_engine.hpp`, executed by
`software/linux/ps_device.cpp` inside `dmotord`. The fixed-point implementation is
in `src/` and `include/dmotor/`; no preparation patches are applied by the product
build. See [architecture](../DMOTOR_ARCH.md) and [build](build.md).

The PL scheduler captures a frame every 20,000 fabric clocks (1 ms at 20 MHz).
The full-frame deadline stays 16,000 clocks (800 us). A worker sleep, crash,
missing/late reply or stale transaction cannot extend the hardware deadline.
The kernel descriptor is exclusive; Runtime and the worker share it but operate
on separate register sets. The worker never grants motion authority or refreshes
application/authorization leases. It starts inhibited, checks identity/cadence,
and asserts HALT on failure. Physical reset requires explicit worker restart;
logical reset invalidates pending work and requires the original cleanup sweep.

## Transaction contract

The original 35-word motor record and 16-word result retain their data layout.
Word 11 operation 3 privately fuses four staging writes, commit and tick into
one PS crossing, executing the original C++ calls in order. Each request carries
a monotonic 64-bit hardware ticket; only one request is pending. All response
words and both ticket words must be staged before commit. PS computes into a
context copy and retains it only after the accepted-ticket readback matches.
Cancellation discards speculative state; no automatic retry/replay or catch-up.
The existing adapter validates the applied motor sequence before target retirement;
the scheduler applies all four duties coherently at a common PWM boundary.

| Byte offset | Access | Meaning |
|---|---|---|
| 0x4000 | R | PSF1 identity, 0x50534631 |
| 0x4004 | R | Period, 1000 us |
| 0x4008 | R | Pending bit 0, fault bit 1, attached bit 2 |
| 0x4010 / 0x4014 | R | Request ticket low/high |
| 0x4018 / 0x401c | W | Response ticket low/high |
| 0x4020 | W | Commit value 1 |
| 0x4024 | RW | Attach value 1, only initially or while engine reset |
| 0x4028 / 0x402c | R | Last accepted reply ticket low/high |
| 0x4080–0x4108 | R | Captured 35-word request |
| 0x4140–0x417c | W | Staged 16-word response |

This private aperture fits the existing 64 KiB AXI mapping. DMI2/DMB1 and the
application-facing `dmotor_client` operations are unchanged. Cancelled response
writes are harmless bus completions; accepted-ticket readback establishes
acceptance. Acceptance is not physical duty application. Logical resets preserve
ticket monotonicity; physical reset removes attachment. Exhaustion fails closed.

## Qualification boundary

Original independent encoder/PWM, STOP deadline, authorization, target age,
physical faults, HALT, monitor heartbeat and CLN1 logic remain in hardware.
The single-motor direct-duty commissioning path is retained. ESP32 firmware and
wire protocol are unchanged. No DRAM DMA/ring or camera subsystem was added.

Main keeps 1 ms; the 5 ms experiment is not selected. The first release remains
commissioning-only and physical tests remain paused. The current monitor recovery
query still reports unavailable; CLN1 alone does not establish operational ARM or
live recovery. Native/co-simulation/ARM build success does not prove the worker's
800 us deadline on hardware under load. Current build evidence is in
[system status](system_status.md).
