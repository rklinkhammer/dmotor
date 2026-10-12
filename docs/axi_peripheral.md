# HLS register peripheral and AXI bridge

`src/peripheral.cpp` implements register decoding and queue operations in portable
C++ HLS. `rtl/axi_bridge.sv` handles only transport, fair arbitration, and the HLS
handshake. `rtl/peripheral_top.sv` connects the actual Vitis-generated ports,
captures a stable hardware context, and masks stale activation after protection.

## Implemented interface (development schema 4)

Identity, schema, capability, clock, ring status, producer/consumer/epoch,
publish-session staging, publication, completion acknowledgment, quiesce, reopen,
and cancellation acknowledgment are implemented. Descriptor writes live at
0x1000, completion readback at 0x2000, active descriptor readback at 0x3000, and
the frozen eight-word cancellation record at 0x3100. Addresses are peripheral
relative. Cancel record format follows command_engine.hpp reply words 8..15.

CAPABILITIES bit 0 indicates the command ring, not motor-control readiness.
ENABLE, watchdog refresh, local/external fault acknowledgment, and staged timeout/
maximum-speed commit are now implemented and connected to RTL safety. See
[safety configuration](safety_configuration.md). CAPABILITIES also sets bit 1.
Motor tuning, telemetry snapshots, and interrupts remain pending. Unsupported writes return SLVERR;
aligned reserved/unimplemented reads return zero. Misaligned accesses and partial
write strobes return SLVERR. Action registers read zero. Active and completion
reads reject invalid/unowned entries. Full descriptor bodies must be initialized
before publication; writes to published slots fail.

DISABLE and STOP close admission/cancel work and emit requests for the future
motion executor. STOP does not itself implement deceleration. These requests are
not protective fault bits: cancellation reasons use bit 31 for disable and bit 30
for controlled stop. The complete safety/lifecycle executor is still pending.

## Transport and autonomy

The bridge buffers AW and W independently, including W-before-AW; it accepts one
outstanding transaction per direction. B and R outputs stay stable until accepted.
Read and write arbitration alternates under simultaneous load, and each completed
bus operation is followed by an autonomous idle poll. Polling also continues
while the processor stalls a response. No software ap_start register exists.

Hardware inputs (timestamp, position, validated configuration, readiness, events)
are captured at HLS launch. Cancellation and completion payloads must remain
stable until their respective acknowledgment pulses; completion tokens reject
stale events. A protective event must independently inhibit the physical output
immediately. Cancellation accounting follows in HLS. The wrapper masks activation
if authorization is revoked or cancellation asserted during a running transaction.

The bridge's 1,024-cycle per-transaction watchdog asserts sticky `fault` and resets
the engine on timeout, rejecting any returned result on the deadline boundary.
A complete buffered bus request gets SLVERR; unmatched AW/W requires coordinated
bus reset. Integration must route `fault` into shared HALT and hold outputs inhibited
until explicit recovery. Resetting HLS clears session state; upstream transaction
isolation/drain is mandatory before reuse. `locally_quiesced` reports only adapter
buffers, never upstream interconnect draining. `upstream_drained` is a trusted
platform input, not a value software can assert without its platform contract.

## Revised proposed response budget

The old eight-clock complete-request-to-response target is superseded for this
HLS-first profile by **2,048 clocks (102.4 us at 20 MHz)**. Measure from acceptance
of AR, or both AW and W, to response VALID; exclude processor response backpressure
and reset. The engine synthesis reports at most 306 cycles, plus adapter launch
and retirement overhead. At most the current transaction, a competing direction,
interleaved polls and the request itself precede a response; five conservative
310-cycle slots fit inside the proposed bound. New engine versions must recheck
this derivation. This is not a placed/routed timing claim or a motor-loop deadline.

The integrated generated-RTL test observed maxima of **199 clocks (1.99 us)** for
both reads and writes and asserts the 2,048-clock bound. Its sequence is finite,
not an exhaustive proof. Separate transport tests exercise split channels,
response stalls, autonomous polling, error propagation and a stuck engine.

## Reproduction

Run `python3 tools/vitis_check.py --kernel peripheral`, then configure CMake again
and run CTest with Icarus available. `rtl_peripheral_integrated` is registered only
when generated peripheral RTL exists. Alternatively use `tools/integrated_rtl.py`
with explicit Icarus paths. Regenerate HLS after any change to its C++ inputs;
CTest alone does not rebuild generated vendor RTL.

The [motion executor](motion_executor.md) now has a separately tested scheduler/PWM
adapter, and the [velocity estimator](velocity_estimator.md) has native/HLS evidence.
The [combined controller](controller.md) now supplies bounded feedback acquisition.
Next connect descriptor dispatch to actual control-boundary activation, completion
retirement and cancellation; add coherent estimator snapshots, tuning commit and
shared fault escalation. No estimator or tuning
registers are added by these standalone kernels. This peripheral is not yet a
complete motor controller or a deployable Vivado design.

The new [HLS dispatcher](dispatcher.md) now connects ring and controller policy
under one owner. This existing AXI peripheral still uses its original ring. Adapting
it to the dispatcher, preserving safety/configuration semantics and remeasuring bus
latency is the next hardware integration step; do not connect two independent rings.
