# Drive-disconnected output validation

This is a manual commissioning procedure, not an automated maintenance-menu test.
Keep motor drive power disconnected, FPGA/encoder logic powered, and connect a
scope or logic analyzer. `--externally-inhibited` is an operator assertion, not a
hardware interlock. Software 0.7.0 adds `--commissioning`; no RTL change is needed.

## Start

Build/deploy the software and image using EDF's `docs/build.md` (use the same
DMOTOR checkout as the hardware). Load the production overlay and driver as in
[production.md](production.md). Do not run inspection UIO against this binding.
Run semantic tests separately before opening the maintenance application.

```sh
sudo dmotord --externally-inhibited /dev/dmotor0 \
  /usr/share/dmotor/output-validation.conf --commissioning \
  > /tmp/dmotor-output-validation.log 2>&1
# Another terminal:
sudo dmotor-maint
```

The example configures only motor 0: 1560 quadrature counts/output revolution
(13 pulses/channel/motor revolution, x4 decoding, 30:1), 10 RPM maximum,
10 RPM/s ramp, proportional-only control and about 5% duty cap. These are
provisional signal-test values, NOT a powered-motor tuning profile. For another
encoder, copy/edit counts_numerator/counts_denominator before restarting the
service. Keep sensor protection enabled. Configuration is read only at startup.

## Signals and normal operation

Probe motor-0 PWM/direction, the host HALT source, shared HALT output and the
actual adapter drive-enable/inhibit signals. Record pin names/polarities and the
bitstream/software identities. Menu commands, entered separately:

1. `m 1`, `r`, `e`, `s`: wait for readiness. Verify zero output at READY.
2. `v 5 0 0 0`: verify ramp, PWM frequency and duty <=3277/65536. Capture startup.
3. With matching synthetic quadrature, verify velocity measurement and direction.
   At 1560 counts/revolution and 5 RPM, expect 130 decoded edges/s (32.5 cycles/s
   on each A/B channel with 90-degree phase). Use FPGA-compatible signal levels.
4. `t`: verify controlled deceleration and eventual zero drive. Log stopped status.
   Repeat `r`, `e` before the next target; STOP requires global rearm.
5. Repeat with `v -5 0 0 0` and reversed encoder phase order. Do not assume a
   stationary/unpowered motor provides the feedback needed for sustained tests.
6. `!`: verify all outputs inhibit. `q`: verify acknowledged all-motor HALT.

Without encoder feedback the 50 ms acquisition or 100 ms zero-speed protections
can trip before the intended timeout. Such a result is NOT a passing target-age
or authorization-watchdog test. Supply continuous appropriate synthetic feedback
and first verify valid, stable tracking without sensor faults.

## Independent timeout injection

Use a fresh `r`, `e`, velocity target and confirmed applied/valid tracking for each
case. Keep the maintenance client connected and servicing its lease.

| Menu | Deliberately suppressed | Still operating | Expected FPGA event |
|---|---|---|---|
| `x` | Periodic velocity publication | Watchdog refresh and client lease | Target-age expiry, shared HALT |
| `w` | Authorization refresh | Velocity publication and client lease | Authorization expiry, shared HALT |

Injection affects all currently tracking/enabled motors, not only the menu mask.
It requires an applied velocity target. Injection cannot be changed or cancelled
in place; HALT/reset/rearm clears it. `!`, quit and client-loss HALT remain active.
Without `--commissioning`, injection is rejected and the service requests HALT.

The FPGA expiry budgets remain 100 ms from the last accepted publication/refresh,
not from the menu key. The host independently HALTs at 120 ms after injection if
no FPGA protection is observed. A log saying `Injection deadline: FPGA trip not
observed; host HALT` is a FAILED FPGA-timeout check, not proof of hardware protection.
Any unrelated sensor/external fault also invalidates that case.

Logs include monotonic host microseconds, event, session, raw status, motor,
position, measured velocity, requested duty and engine/protection faults. Periodic
snapshots are requested every 20 ms; fault status is sampled before runtime HALT.
The startup banner and event/error lines mean this is a mixed log, not a pure CSV.
A blocked log sink can delay the service; keep logs on local storage. Neither
host timestamps nor software duty telemetry establish electrical timing.

## Acceptance record

For each case save scope traces and logs, source-version.json, hardware-inputs.json,
configuration, encoder stimulus and observed fault reason. Record PWM period/duty,
direction, STOP time and external-HALT-to-logical-inhibit delay (budget 10 us).
Measure physical adapter turn-off separately. Confirm shared inhibition on all
four channels, external fault assertion, client quit/crash/stall, service death,
and that reconnect cannot resume motion without explicit recovery.

No automatic PASS is generated. Native/fake-device tests validate software
semantics only. A reviewed hardware record and electrical/current-limit checks
are required before implementing/enabling a powered commissioning mode.

Software validation results and deployment limitation: [evidence](../evidence/output-validation/README.md).
