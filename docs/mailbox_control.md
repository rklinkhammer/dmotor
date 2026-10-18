# Mailbox-to-scheduler integration

> Current product builds: [build guide](build.md). Commands below are specialized
> component/experiment reproduction or historical evidence, not the default build.


`rtl/mailbox_control.sv` connects the generated C++ HLS atomic mailbox through
`rtl/atomic_mailbox_arbiter.sv` to the real shared scheduler, target-age protection
and four PWM blocks. Motor-engine ports remain normalized; the shared velocity
engine and its encoder/configuration adapter are the next integration milestone.

Mailbox validation, publication, reservation and retirement remain C++ HLS. The
arbiter implements clock-level ownership of the single HLS invocation interface;
it does not duplicate the mailbox state machine or motor calculations.

## Interface and arbitration

The host interface is a single 32-bit read/write request with byte strobes and
valid/ready, followed by one buffered response. It is not AXI. Requests cannot
invoke the internal reset, begin, retire or invalidate operations. A stalled host
response prevents another host request, but does not hold the HLS transport or
block safety/scheduler operations. The buffered response remains stable until
consumed. Scheduler held requests produce exactly one invocation until deasserted.

Priority after the active invocation drains is:

1. Trusted initialization after reset, using the nonzero supplied session.
2. Pending invalidation.
3. Scheduler retirement.
4. Scheduler admission for the captured effective tick.
5. Host register read/write.

The wrapper inhibits output until initialization completes. Authorization is
intersected with the enabled mask; generations, maximum speeds and readiness are
supplied by the future engine/lifecycle adapter. A changed session requires a
shared inhibited reset; it never silently changes ownership. Independent physical
fault qualification and authorization watchdogs must feed the external inhibit.

## Cancellation and live-state changes

Explicit invalidation, a canceled/aborted frame, newly asserted inhibit, and
changes to authorization/configuration invalidate work. Configuration/authorization
changes conservatively flush all four mailbox channels in this first adapter.
Invalidation events received during another invalidation are retained. A held
invalidation mask blocks new normal work until released and drained.

A running HLS call is drained. Its launch snapshot is compared against live
session, authorization, generations and speed limits before accepting its reply;
readiness is also checked before exposing a reserved batch. Cancellation is
qualified combinationally on the response-consumption edge, so a simultaneous
cancel cannot release a successful scheduler response. Canceled reservations are
invalidated before another frame is allowed to start. No-batch ticks continue
existing targets without renewing target age or retiring a batch.

Retirement needs special care: the HLS core may already have updated its applied
metadata when cancellation arrives. A canceled/stale retirement therefore latches
a protocol fault, blocks new host access and inhibits all outputs until reset.
It does not pretend that invalidation can roll back applied history. Successful
retirement still precedes PWM release, as in the scheduler contract; applied
metadata is protocol retirement evidence, not a physical motion measurement.

The transport execution timeout propagates to shared HALT. Its named cycle bound
is separate from the scheduler's complete frame deadline. At most one already
accepted host operation can delay a newly requested scheduler operation; queued
host requests cannot preempt it.

Frame cancellation is not a complete controlled-stop implementation. The engine
adapter must still deliver stop/disable lifecycle operations and handle any HLS
motor state updated before a frame was canceled. The independent inhibit path
protects outputs while that integration remains incomplete.

## Verification and remaining work

`tests/mailbox_control_tb.sv` uses the actual generated atomic-mailbox RTL,
arbiter, scheduler, target-age timers and PWM. Only the motor computation responses
are behavioral. Tests cover four-motor atomic application, one invocation per
held scheduler request, host response backpressure across a complete frame,
no-batch ticks, canceled reservations, generation-change escalation, cancellation
on the retirement response edge, transport timeout and session-change protection.
The test uses accelerated frame/PWM periods; production defaults remain 20 MHz,
1 kHz control and the named timing constants.

```sh
python3 tools/vitis_check.py --kernel atomic
python3 tools/atomic_adapter_rtl.py --test mailbox_control
cmake -S . -B build/atomic_protocol
ctest --test-dir build/atomic_protocol --output-on-failure
```

This is not an integrated motor-controller bitstream. Remaining work includes the
actual shared motor-engine adapter, qualified encoder snapshots, lifecycle/STOP
control, host AXI integration, full safety-path verification and board integration.
No new resource estimate or routed timing result is claimed for this wrapper.

The next milestone has begun with the [shared-engine transport](shared_engine_transport.md).
It verifies actual motor HLS ports separately; this wrapper's motor connection
remains behavioral until warm-up/readiness and per-motor publication are adapted.
