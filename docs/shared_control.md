# Shared scheduler, protection and PWM boundary

`rtl/shared_control.sv` connects an autonomous four-channel scheduler, the real
independent target-age module, and four phase-aligned PWM output blocks. It exposes
normalized protocol and engine handshakes for the next generated-HLS integration.
It is not a board top or a complete connection to the atomic-mailbox/motor HLS IP.

The algorithms remain C++ HLS: mailbox validation/management, velocity estimation,
ramp/PI, and per-motor control state. RTL here handles clock-level scheduling,
coherent capture, result gating and protection. It contains no replacement motor
control algorithm. Existing RTL PWM remains the tested reference output block.

## Frame transaction

At the generated 1 kHz tick, capture enabled mask, session, configuration generations
and all encoder snapshot payloads. The snapshot payload width is a parameter and
opaque to this module; its actual field packing belongs to the engine adapter.
The default synthesis measurement uses 256 bits per channel, not a frozen encoder
record format. Timestamp comes from an independent free-running counter in the
scheduler and continues even if HLS does not complete.

1. Hold `admission_request` until a protocol response arrives. Its adapter must
   reserve a pending atomic batch for this effective tick, or report no new batch.
   The existing target continues on ticks without a batch; those ticks never renew
   target age. Admission and retirement adapters each allow only one transaction
   in flight and must convert level requests into one HLS invocation.
2. Service enabled motors in ascending index order. Hold engine start until ready;
   hold motor/session/generation/tick/target and snapshot throughout the invocation.
   Accept completion only for an accepted start, with matching returned tags.
   Generated-HLS adapters must echo captured launch tags and present one-shot done.
3. Store each duty in a staging bank. No partial bank is visible to PWM. All selected
   calculations must succeed before requesting target-age acceptance for a new batch.
4. On target-age acceptance, request protocol retirement. On successful retirement,
   or after ordinary calculations without a batch, wait for the common PWM boundary.
5. Present all staged duties before that sampling edge and latch the whole bank
   together. PWM direction-reversal blanking remains independently enforced per
   motor, so atomic duty intent does not promise simultaneous mechanical reversal.

The protocol response must be validated against the captured session, mask and
configuration generations. The future adapter must not prematurely report applied
sequence before successful retirement. New batches affect selected channels only;
all enabled channels still run their periodic control step.

## Priority and failure behavior

External inhibit, target-age inhibit, missed deadline, overrun, bad result tags,
engine fault, changed session/configuration or failed protocol/age retirement
prevent result release. Scheduler faults latch and request shared HALT; recovery
requires inhibited reset/drain, not merely a later successful result. External
inhibit and current fault conditions bypass PWM phase immediately.

`inhibit` is the external protection input (authorization watchdogs, physical faults,
HALT). Target-age inhibit is connected separately. Prospective age rejection must
not combinationally gate its own apply-valid signal; this separation avoids a
valid/reject feedback loop. Registered acceptance gates subsequent retirement.

The frame deadline covers admission, engine handshakes/calculation, target-age
acceptance and protocol retirement. A completion at the deadline loses. A subsequent
control tick arriving while a frame is still pending is an overrun. PWM release
follows successful calculation/retirement at the next boundary, up to one PWM
period later. `last_latency`/`maximum_latency` count through validated retirement,
not physical motor response. Period/deadline constants default to generated
20 MHz, 1 kHz, 800 us and 20 kHz values.

`cancel_frame` or a changed enabled mask discards the current frame without claiming
it completed. An already accepted engine call is drained, discarding its result,
rather than resetting all peer HLS contexts; a stuck drain still hits the deadline.
Disabled channels clear retained duty so re-enable cannot replay an old command.

Cancellation is a trusted integration action, not a complete controlled-stop
implementation. The adapter must invalidate the entire intersecting protocol batch,
close admission and inject the selected stop/disable transition into the HLS motor
contexts before they next calculate. It must drain outstanding protocol operations
as well. `stop_or_ready` disarms the corresponding target-age timers. A standalone
cancel pulse does not implement ramp-to-zero or a stop deadline, and retains previous
peer duty until the next valid frame. Faults still override cancellation.

`frame_aborted` tells the adapter to invalidate in-flight protocol intent; historical
applied records remain diagnostic and are not proof of a PWM update. All final
outputs are also gated by per-channel enable. External fault qualification, watchdog
refresh/recovery registers, shared HALT aggregation, electrical adapter behavior,
and generated-HLS port adapters remain integration responsibilities.

## Verification

`shared_scheduler_tb.sv` instantiates the production `dmotor_shared_control`, actual
target-age and actual four-channel PWM modules. It supplies behavioral protocol and
engine handshake responses; it does not simulate the generated mailbox/control IP.
Tests cover captured snapshots, start held until ready, different engine latencies,
all-channel common PWM application, no partial frame, disabled-channel isolation,
ordinary no-batch ticks, wrong result tags, session changes, age/retirement refusal,
late deadline completion, stalled arithmetic, cancel/drain, cancellation before
PWM, external inhibit between PWM boundaries and disable/re-enable replay prevention.
Timing is accelerated using named test parameters; production timing defaults are
used in standalone synthesis, not a measured end-to-end four-HLS-call run.

All 27 regression tests pass, with the additional disabled-duty replay test also
passing after its implementation. Evidence is retained under
`docs/evidence/shared_control/`. The existing full-profile HLS sources and ABI remain
unchanged. The next step is explicit adapters for the atomic-mailbox and shared
C++ engine, then generated-RTL closed-loop integration tests and complete utilization.

```sh
cmake -S . -B build/atomic_protocol
ctest --test-dir build/atomic_protocol --output-on-failure
/opt/vitis/2026.1/Vivado/bin/vivado -mode batch -source tools/synthesize_shared_control.tcl
```

The synthesis script includes the scheduler, target-age and four PWM blocks, with
ideal zero boundary delays and no integrated clock-skew model. Its reports exclude
the HLS engine, protocol IP, AXI, encoder capture and authorization watchdogs. Final
resource fit, hold correction and timing closure require integrated placement/routing.

Standalone Vivado 2026.1 OOC synthesis on xc7z020clg400-1 reports **3,167 LUT,
2,872 FF, zero DSP/BRAM** for this boundary, including target-age protection and
four PWM blocks. Do not add the separate target-age estimate again. Setup slack
is +33.808 ns at 20 MHz; hold slack is -0.327 ns under ideal boundary assumptions.
The report does not establish timing closure; routing/hold correction and complete
clock constraints remain outstanding. These figures exclude both HLS IP blocks.

## Generated mailbox transport progress

The [atomic HLS transport](atomic_hls_adapter.md) is now tested separately against
the generated mailbox RTL. Its request/response interface still needs an arbiter
to connect these scheduler admission/retirement requests and host operations.
The scheduler bench continues to use behavioral protocol and engine responses.
