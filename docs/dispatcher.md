# C++ HLS queue/controller dispatcher

`dmotor_dispatcher` owns one `CommandRing` and one combined `Controller`. It
serializes internal queue-service requests with autonomous 1 kHz control ticks.
This connects queue policy to actual controller acceptance in synthesizable C++;
it is not yet an AXI-connected full FPGA top.

## Offer, accept, capture

`CommandRing::offer()` reads an immutable published descriptor without advancing
its consumer or reserving additional state. It only offers when admission is open,
no command is active and completion capacity is available. The dispatcher validates
the offer against the actual queue counter, epoch, configuration and current position.

At a hardware tick it runs the estimator/controller with that candidate. Only if
the controller accepts it on **this tick** does the dispatcher commit ring capture
using the same timestamp and position. Current feedback acquisition, a held
completion, stop or protection can block acceptance; yesterday's ready flag never
commits capture. Relative position resolves at this boundary. The single serialized
owner guarantees that publication/capacity cannot change between offer and capture.

Invalid descriptors are consumed as rejected completions only when the controller
is ready, with zero activation timestamp and the actual queue sequence/epoch. They
never become controller commands. A full completion ring blocks offers but every
autonomous estimator/control tick still runs. Completion acknowledgment permits
progress without interrupting existing velocity regulation.

When an accepted command completes, the dispatcher checks active sequence and
epoch, records the controller's outcome, detail and terminal timestamp, and
acknowledges the controller completion on the next tick. Internal completion-token
mismatch is a sticky protective fault. Immediate commands can capture and complete
on the same tick; position, dwell and queued stop retain their reservation until
completion or cancellation.

## Stop, protection and lifecycle

Live cancellation, capture fault or loss of authorization precedes request mutation,
activation and retirement. The ring immediately records the active aborted
completion and pending cancellation range at the service timestamp. A pending
flag carries the event to the next control tick. The controller's subsequent abort
record is acknowledged locally, never retired a second time.

Immediate controlled stop also cancels the active/pending queue, closes admission,
and is held until a tick consumes it. It retains authorization so the controller
can perform its existing measured controlled stop. Protective cancellation overrides
that request. These internal inputs are events and must be delivered exactly once
(or held only until service acknowledgment by the future hardware adapter).

Decoder capture faults latch even if their input pulse clears before the next tick.
Controller faults cancel the ring and zero visible duty. External watchdog/HALT
must independently inhibit outputs and revoke authorization without waiting for
HLS. The dispatcher does not implement physical protection pins or a watchdog.
Any nonzero returned controller/dispatcher fault must request shared HALT in the
final wrapper. Ordinary disable/cancellation closes the queue and inhibits drive
without inventing a new local protective fault.

Reopening requires live authorization false, a completed unauthorized control tick,
no pending stop/cancel event, no sticky fault, a nonzero configuration generation,
and the ring's existing drain/empty/completion/cancellation prerequisites. Session
increments on reopen; no stale command automatically resumes. Recovery from sticky
controller faults currently requires a coordinated reset and upstream isolation/drain.
Configuration must remain fixed until coordinated controller/estimator reset; the
validated tuning-commit/recovery adapter is still required.

## Internal transaction interface

Requests use the existing four-word `EngineOperation` protocol: poll, stage,
publish, reopen, quiesce, completion acknowledgment, cancellation acknowledgment,
completion-word read and active-descriptor read. **Poll does not create a control
tick.** The separate `tick` input is supplied by an autonomous hardware scheduler;
there is no software tick register. CPU request traffic must never starve it.
A request and tick in the same service are ordered request first, then tick, unless
protection or stop takes priority. This allows a completed publication to become
eligible at that boundary.

Configuration is the combined controller's 20-word internal layout. Replies 0..15
retain the [command-engine layout](command_engine.md); replies 16..39 contain the
[combined-controller reply](controller.md). Reply 6 pulses only for an accepted
activation, not for a rejected descriptor. Reply 32 includes sticky controller
faults; bit 31 additionally denotes a dispatcher invariant/token failure.
Controller-generated cancellation uses cause `0x20000000 | fault_mask`; immediate
stop uses `0x40000000`, and loss of authorization uses `0x80000000`. The cancellation
record's cause is distinct from the controller fault register.

Between ticks, control telemetry is the cached last result, with accepted cleared
and duty masked for inhibition/pending protection. Completion telemetry is not an
instruction for a second ring owner to retire it: this dispatcher already owns
retirement. Keep tick timestamps exactly 100,000 fabric clocks apart. Queue-only
services may occur between them and do not advance estimator/control history.

## Integration boundary and evidence

The future AXI/safety wrapper must route register requests to this owner and latch
coherent encoder/tick snapshots, give ticks bounded scheduling priority, transfer
retired duty to PWM, mask late results with live protection, and propagate faults
to shared HALT. It must replace the existing peripheral's private command-engine
ring rather than instantiate two queues. Public MMIO and AXI timing have not been
changed by this internal kernel. Re-evaluate the 2,048-clock AXI response budget
when adapting it; older peripheral timing evidence does not prove this new path.

`dispatcher_test` drives the actual HLS top for native and Vitis co-simulation. It
covers initial qualification without early capture, exact activation/terminal times,
velocity/dwell/queued stop ordering, full completion pressure with continuing duty,
immediate stop cancellation without duplicate completion, drained reopen, descriptor
rejection, current-tick invalid feedback, missed ticks, protective priority, a
between-tick capture fault and suppression of acceptance telemetry when a
controller fault prevents ring capture. This is generated kernel evidence, not a complete
AXI-to-PWM hardware test or physical motor validation.

Run `python3 tools/vitis_check.py --kernel dispatcher`. The synthesis result already
contains ring, estimator, profiles and PI; do not add standalone component estimates
to it. See [verification results](verification.md).

## Single acceptance-boundary validation

The dispatcher validates an offered descriptor once per tick, using the live
position, ring consumer/session and active configuration. Descriptor generation
must also match the ring's committed generation. Only a successful result is
passed to the motion executor for possible acceptance; a rejected descriptor is
retired when the current controller result is ready, with zero activation time.

Private friend-only implementation paths let the controller/motion executor and
ring capture reuse this result during the same serialized call. No software
input or public HLS port can assert that a command is already validated. The
immutable published slot cannot change between offer and capture. Capture still
checks admission, busy state and completion capacity, and completion correlation
still comes from ring counters/session rather than untrusted descriptor fields.
Standalone controller/motion entry points and public ring capture retain full
validation. HLS arithmetic-sharing directives target the private motion
implementation, preserving the prior exact-divider optimization.

Cancellation, live feedback faults and deadline/configuration faults still take
precedence over activation. Validation is repeated on later ticks if the command
was not accepted; it is never cached across changing position or configuration.
A ring/configuration generation disagreement is now rejected before activation,
rather than permitting provisional motion acceptance followed by capture rejection.

## State update and telemetry implementation

Tick processing now uses a local controller result, applies retirement/fault
handling, and commits `last_` once. Reply encoding reads that snapshot directly;
only the duty, motion-status and controller-fault words receive the existing live
inhibit/activation masks. This avoids a second full result-structure copy without
changing any reply field or between-tick masking behavior.

The dispatcher Vitis configuration preserves and shares `MotionExecutor::finish`
and `MotionExecutor::fail` as HLS functions. This reuses their state-update logic
across branches while retaining C++ call order and fault priority. It does not
share hardware across motors or change the public interface. These directives
are dispatcher-specific: standalone motion/controller builds retain their previous
settings because the same sharing worsens their clock-uncertainty margin.

Reproduce the complete native reply comparison with:

```sh
python3 tools/check_dispatcher_equivalence.py --reference a81357a
```

The script compiles the current and reference dispatcher source against the same
current dependencies/testbench, enables test-only trace output, and compares all
40 reply words for every service call. It checks dispatcher-source equivalence,
not all possible inputs or hardware timing.
