# Velocity mailbox measurement

> Current product builds: [build guide](build.md). Commands below are specialized
> component/experiment reproduction or historical evidence, not the default build.


This isolated experiment replaces the velocity-only dispatcher's command and
completion rings with an atomic velocity setpoint mailbox. Main production
sources and the architectural contract are unchanged. The helper builds a
separate candidate using the bounded exact velocity profile and one-phase divider.

## Matched synthesis results

Vitis 2026.1, `xc7z020clg400-1`, 20 MHz; both candidates use identical arithmetic,
protocol, motion, velocity, and controller source files.

| Resource / timing | Fresh ring baseline | Mailbox | Change |
|---|---:|---:|---:|
| LUT | 20,794 | 17,589 | -3,205 (-15.4%) |
| FF | 10,439 | 7,191 | -3,248 (-31.1%) |
| DSP | 20 | 20 | 0 |
| BRAM18K | 4 | 0 | -4 |
| HLS maximum latency, cycles | 470 | 324 | -146 |
| HLS maximum latency, microseconds | 23.50 | 16.20 | -7.30 |
| Estimated critical path, ns | 34.572 | 35.251 | +0.679 |

Both meet the 36.50 ns HLS timing budget after clock uncertainty. These are HLS
estimates, not placed-and-routed utilization or hardware qualification. Co-simulation
observed a 302-cycle maximum for the mailbox test sequence; the table uses the
more conservative synthesis bound.

The fresh ring baseline is smaller than the historical 20,935-LUT report because
the active source now also uses the one-phase wide encoder divider. The 141-LUT
and 64-cycle baseline improvement is not attributed to the mailbox.

Four independent mailbox instances would total 70,356 LUT, already above the
53,200-LUT device capacity before integration logic. This supports measuring a
shared multi-channel engine next; this experiment does not demonstrate four-motor fit.

## What changes

The FPGA retains encoder velocity estimation, exact PI arithmetic, acceleration
and deceleration ramps, feedback qualification, and controller fault handling.
Position planning, dwell sequencing, and command history belong on the processor
in this proposed profile. The mailbox has no position command or completion FIFO.

A mutable four-word staging bank holds session, configuration generation,
sequence, and signed milliRPM target. A complete commit atomically copies these
to a protected pending bank. The existing controller holds the active target.
A newer committed target supersedes an unapplied pending target. Partial staging
cannot alter pending state. Application waits for controller readiness and is
reported through an applied-sequence watermark; skipped sequence numbers have
no individual completion record.

Commit requires all four fields, matching nonzero trusted session and configuration
generation, increasing sequence, and speed within configuration limits (200 RPM
in this candidate). Sequence does not wrap within a session. Trusted session or
configuration transitions must occur inhibited, with reset/reinitialization as
needed; the wrapper owns these tokens. Ownership is checked again at activation.
Rejected commits preserve the previous pending frame and require fresh staging.

Out-of-band controlled stop clears staging validity and pending work, blocks new
publications during the stop, and uses the existing ramp to zero. Authorization
loss or a captured protective fault clears pending work. Capture faults remain
latched until reset. Protection takes priority over stop, and stop over publication.
The experiment holds the last target while authorization remains valid; it adds
no setpoint-age timeout. A deployment must explicitly approve that freshness
policy or specify a separate timeout.

This removes ring management and reduces telemetry, so the LUT difference measures
that combined interface simplification, not ring storage alone. It still reuses
`Controller` and internal descriptor/completion metadata. Further specialization
and configuration-state simplification remain possible.

## Experimental transaction interface

`dmotor_mailbox` is an internal hardware-scheduled HLS transaction interface, not
an AXI peripheral or frozen public ABI. Request words are:

| Operation | request[0] | request[1] | request[2] | request[3] |
|---|---:|---:|---:|---:|
| Poll | 0 | ignored | ignored | ignored |
| Stage | 1 | field index 0..3 | value | 0 |
| Commit | 2 | 0 | 0 | 0 |

Reply words 0..3 report request acceptance, pending validity, applied sequence,
and pending sequence. Words 4..9 report duty, ramp milliRPM, measured milliRPM,
status, faults, and sticky rejection reasons. Words 10..11 hold sample age;
12 is the published-sequence watermark; 13 is the trusted session; 14..15 are zero.
See the experiment source for bit definitions.

Independent hardware watchdog, deadline enforcement, physical output inhibit,
and shared HALT propagation remain required. Gating the returned duty on a service
call is not an asynchronous safety path. This experiment does not add their
mailbox integration, a host driver, AXI retirement handling, or recovery ABI.

## Verification and reproduction

All 15 applicable native/RTL tests pass. The new test compares 740 control snapshots
against the existing Controller oracle: duty, ramp, measured velocity, feedback
status, faults, and full sample age. It separately checks atomic publication,
partial staging, supersession, replay and ownership rejection, speed bounds,
sequence exhaustion, delayed activation, direction reversal, stop priority,
authorization loss, captured faults, and reset. Both measured tops pass HLS C
simulation and Verilog co-simulation. Existing protection RTL tests pass, but
there is no end-to-end mailbox/AXI/protection integration test yet.

From the repository root, using fresh output directories:

```sh
python3 tools/prepare_bounded_velocity.py --variant exact-onephase --output build/mailbox_ring_baseline
python3 tools/prepare_velocity_mailbox.py --output build/velocity_mailbox
cmake -S build/velocity_mailbox -B build/velocity_mailbox/build
cmake --build build/velocity_mailbox/build -j
ctest --test-dir build/velocity_mailbox/build --output-on-failure -R '^(arithmetic|generated_abi|control|motion|velocity|controller|dispatcher|mailbox|rtl_safety|rtl_scheduler|rtl_qcencoder|rtl_pwm|rtl_latency|rtl_axi_bridge|rtl_encoder_snapshot)$'
python3 build/mailbox_ring_baseline/tools/vitis_check.py --kernel dispatcher
python3 build/velocity_mailbox/tools/vitis_check.py --kernel dispatcher
```

The mailbox helper intentionally reuses the dispatcher runner/configuration slot
with top `dmotor_mailbox`. It copies the versioned experiment files and leaves
main sources unchanged. A second fresh preparation was checked byte-for-byte
against the measured candidate's mailbox source, header, test, CMake and HLS config.

Reports and native test output: [evidence/velocity_mailbox](evidence/velocity_mailbox/).
