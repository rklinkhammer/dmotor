# Combined C++ HLS controller tick

`dmotor_controller` combines the existing velocity estimator and motion executor
in one hardware-scheduled C++ HLS transaction. It consumes a coherent encoder
snapshot and one pending command mailbox at each 1 kHz control boundary. It is
not yet the AXI ring dispatcher, a complete FPGA top, or a board-ready controller.

## Feedback acquisition policy

On initial authorization, commands remain pending and duty stays zero until the
estimator supplies a qualified measurement. This can be a measured period or the
explicitly tagged timeout-qualified zero after the configured no-edge interval.
The estimator continues to run while unauthorized. A reset/reconfiguration starts
fresh estimator history; authorization loss always removes drive.

After qualification, loss of validity without an estimator fault enters bounded
acquisition. This covers the first edge after timeout and the first reversed
edge. During that interval:

- The profile, PI integral and settling progress do not advance. Settling history
  is cleared so a stale partial settling window cannot complete a move.
- The last computed duty is held, capped symmetrically by the configured acquisition
  duty limit. This is an explicitly degraded drive mode, not valid speed feedback.
- No new command activates. A held completion can still be acknowledged; an
  active motion timeout continues running.
- An immediate controlled stop removes held drive on this control tick, remains
  incomplete, and resumes measured deceleration/settling only when qualified.
- A valid measurement resumes ordinary control only before the acquisition deadline.
  Expiration wins on the deadline tick, even if a new period qualifies then.

Acquisition time is anchored to the first invalid tick. Repeated reversal events
cannot restart it. A timeout, capture fault, estimator fault, invalid-transition
count change or motion fault latches a controller fault and requests zero output.
Recovery requires explicit reset and fresh lifecycle authorization through the
future dispatcher. External watchdog/HALT hardware must continue to inhibit
immediately, independently of this scheduled computation.

The standalone `dmotor_motion` interface retains its strict invalid-feedback
behavior. Only this coordinator passes the internal `feedback_pending` argument;
callers must not use that argument to authorize an unbounded hold. The estimator's
public validity and age remain truthful throughout acquisition; no synthetic zero
or stale value is marked as a fresh valid measurement.

The acquisition timeout must be at least one tick and **less than the estimator
zero timeout**, preventing a timeout-qualified zero from silently ending a failed
acquisition. Its duty limit is between zero and the configured ordinary duty limit.
The tests use 60 ms acquisition, 100 ms zero timeout and a duty cap of 128/65536.
Those are synthetic verification values, not validated motor tuning or production
power limits. An actual vehicle must establish useful duty limits, acceptable
coasting/deceleration and whether this hold policy meets its stopping requirements.
The coordinator does not diagnose a stuck encoder from missing edges alone.

## Internal wire contract

The top receives reset, live authorization, decoder capture fault, fabric timestamp,
accepted-event count, latest edge time, direction epoch, invalid-transition count,
direction, signed position, session, command-valid, a 16-word descriptor,
completion acknowledgment and immediate-stop event. All are coherent and stable
for the HLS transaction. A command remains pending until acceptance is returned;
stop and acknowledgment are single sampled events. Live protection must mask a
late retired result and cancel queued/active work separately.

Configuration words 0..13 match the [motion configuration](motion_executor.md).
The remaining words are:

| Word | Meaning |
|---|---|
| 14 | Estimator zero timeout, fabric clocks |
| 15 | Feedback acquisition timeout, fabric clocks |
| 16 | Acquisition duty cap, signed Q16, nonnegative |
| 17..19 | Reserved, must be zero |

All configuration remains fixed until reset/reinitialization. The combined profile
inherits the estimator's CPR denominator limit of 10,000. This interface does not
add public MMIO registers; validated tuning/configuration commit remains work.

Reply words 0..11 match the motion reply (duty, ramp, command flags, completion and
timestamps). Additional words are:

| Word | Meaning |
|---|---|
| 12 | Signed estimated milli-RPM |
| 13 | Estimator status: valid bit 0, timeout-zero bit 1, averaging bit 2, fault mask at bit 8 |
| 14 / 15 | Estimator sample age, low / high fabric clocks |
| 16 | Sticky controller fault mask |
| 17 | Acquisition bit 0, initial warmup bit 1 |
| 18..23 | Zero |

Controller fault masks: `1` acquisition configuration, `2` decoder capture fault,
`4` new invalid-transition diagnostic, `8` estimator fault, `16` acquisition deadline,
`32` motion executor fault. The motion and estimator sub-status preserve details.
An active motion aborted by coordinator protection returns its existing
unauthorized-abort detail (`0x100`); controller fault bits identify the cause.
Final integration must route **every nonzero controller fault to shared HALT** and
perform authoritative ring cancellation before accepting any completion token.
No hardware HALT_OUT connection is claimed by this standalone combined kernel.

## Verification and remaining integration

The same `controller_test` drives native C++ and Vitis RTL co-simulation. It checks
startup without false acceptance, first-edge acquisition, reversal, deferred
command acceptance, invalid-transition protection, deadline-boundary precedence,
non-extensible deadlines, controlled stop during acquisition, authorization loss,
capture faults and acquisition configuration validation. Encoder input is synthetic;
these tests do not establish closed-loop motor dynamics or vehicle performance.

Run `python3 tools/vitis_check.py --kernel controller`. This top already contains
estimator, profiles and PI; do not add separate motion/velocity IPs in series.
See [verification evidence](verification.md). The new [dispatcher](dispatcher.md)
now owns the ring/controller acceptance and completion sequence in C++ HLS.
Next adapt AXI and wire collector/scheduler/PWM and shared safety, then implement
tuning MMIO and motor-model tests. Integrated resource fit and routed timing remain
requirements.
