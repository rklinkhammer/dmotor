# Best-effort monitor workload and policy

[D10](decisions.md#d10--best-effort-motor-overload-protection-and-accepted-motor-damage-risk)
accepts motor damage from delayed current detection/shutdown, assuming verified
ITRIP. No motor-survival deadline is claimed. This does not waive independent
stop/fault behavior, missing-data handling, controlled recovery, or wiring/battery
constraints. Hardware interfaces and the 1 ms PS loop are unchanged.

## Implemented path

`monitor_cycle.hpp` owns one complete bounded workload: normalized fault inputs,
both battery ADCs plus IPROPI for each explicitly installed motor, six INA reads
per installed device (identity/configuration/mask/shunt/bus), calibration, and
policy evaluation. At four motors this is six ADC reads and 24 I2C reads. Identity
and configuration remain checked each cycle. Population masks must agree across
sensor, calibration and policy configuration. Missing devices are never removed.

ADC work must finish before release+1 ms; INA work before the earlier of ADC-end
+10 ms and release+11 ms; policy/commit before release+12 ms. Backwards clocks,
clipped/failed ADCs, incomplete or stale INA data, and invalid calibration cannot
produce a healthy decision. One initial inhibited conversion-ready baseline is
allowed; later warming cannot clear a fault. No default calibration is fabricated.

`monitor_policy.hpp` implements:

- Per-motor **IPROPI winding-current proxy** limits with elapsed-time persistence
  and release hysteresis. A low INA reading cannot cancel this fault.
- Separate absolute **INA supply-current** limits (either sign), also with
  persistence and hysteresis; these are not winding-current measurements.
- A sustained-high-IPROPI stall proxy. Optional motion-informed mode additionally
  requires commanded motion and no observed movement; absent/stale/future motion
  data inhibits. It does not assume an encoder path exists on ESP32.
- Battery full/midpoint plausibility, configured pack range, per-2S undervoltage
  persistence and recovery threshold, and independent imbalance persistence.
- Immediate polled hard-input faults, invalid data/configuration and cycle timing
  faults. Causes and affected motor mask latch; good samples do not auto-rearm.

Threshold excursions begin on the first observed violating sample; elapsed time
includes any hysteresis-band samples until the signal returns to the release
level. This is not continuous waveform observation. Threshold equality trips,
release equality clears the pre-trip timer; latched faults remain latched.
Without motion evidence, stall is explicitly a current/time proxy, not proof of
a blocked rotor. All test thresholds in `monitor_policy_test.cpp` are synthetic
fixtures, **not approved hardware values**. Configuration defaults are invalid.

`CurrentCalibration` is a separate versioned, board/population-bound CRC32 record
for four IPROPI raw-count-to-mA gains/offsets. Existing INA/battery calibration
format is unchanged. Missing, corrupt or incompatible records inhibit. Storage,
known-reference coefficients and reviewed board thresholds remain integration
work; acceptance of motor damage does not define those numbers.

## Liveness versus snapshot age

The supervisor still requires a sample age <25 ms at final cycle commit.
It no longer revokes a good decision solely because that sample crosses 25 ms
between scheduled cycles. The next release's 12 ms work deadline is the independent
liveness cutoff, even if the next worker never begins. In-flight worker tokens
carry fault epoch and cycle identity; an intervening fault defeats late results.

The measured age bound of 20.240 ms therefore no longer forces a 4.760 ms permit
window. This is an adopted best-effort policy, not a tighter physical age proof.
The configured conversion-window bound still matters for snapshot validity, and
stale-but-readable devices must still be rejected. A successfully read stale
register must not be treated as a new conversion.

## Heartbeat is independent of motor permission

Heartbeat is a dedicated 3.3 V GPIO21 signal, not UART data. Each completed,
fresh, calibrated, on-budget monitoring/policy evaluation toggles the modeled
level once. With a 16 ms loop that is nominally a transition every 16 ms and a
31.25 Hz full waveform. Actual edge spacing follows completion-time jitter; this
is not a free-running timer. A LOW phase of a healthy waveform does not mean the
monitor is dead; consumers must observe transitions.

| Current monitoring result | Motor authorization | Modeled HB | Modeled permits |
|---|---|---|---|
| Valid evaluation, policy clear | Disarmed/recovery incomplete | Toggles | LOW |
| Valid evaluation, policy clear, no latched fault | Explicit ARM/recovery satisfied | Toggles | Eligible |
| Valid evaluation reports overload/STOP or a prior operational fault remains latched | Any | Toggles | LOW |
| Missing/stale/invalid inputs, calibration failure, incomplete/late work | Any | Stops, driven LOW | LOW, fault latched |

`PolicyResult.monitoring_complete` means evaluation completed with valid current
inputs, not that policy approved motion. `SafetyProof.policy` retains the separate
approval result. The pipeline continues evaluating inputs even after a latched
operational fault, so the latch neither fabricates nor suppresses monitoring
health. Input failure cannot be hidden behind an already-latched overload.

The supervisor permits new monitoring cycles while motor faults remain latched.
Fresh completed work can resume HB, but cannot clear the fault or restore permits.
An intervening fault invalidates an in-flight token; only a subsequent cycle may
resume HB. Backwards-clock/exhaustion failures remain locked. Liveness expires at
next scheduled release+12 ms even when disarmed, fault-latched or in HB's LOW phase.
Duplicate completions and repeated release timestamps cannot manufacture HB edges.
`finish()` still returns motor permission; false does not necessarily mean failed
monitoring. `monitoring_live()` exposes qualified monitoring separately.

The physical adapter still forces **both HB and permits LOW**; modeled HB is not
yet wired to GPIO. No new pin/voltage, UART format, PL timer or external watchdog
setting is introduced. Operational HB output and electrical timing qualification
remain pending. U1 `hb10` remains a separate synthetic 50 Hz test waveform.

## ESP32 integration

`EspMonitorInputs` maps the existing GPIO1/2 battery ADCs, GPIO4–7 IPROPI ADCs,
STOP40 (healthy low), HALT39 (active high), ALERT47 (active low), and installed
nFAULT12/13/14/42 (active low). It uses no new pins, pull-ups or wire protocol.
It polls faults before/after acquisition; **polling is not the required ISR path**.

`EspMonitorTask` provides a single-owner 16 ms worker using the absolute release
wait, the physical ADC/I2C adapters, policy pipeline, and separate high-priority
supervisor. The application supplies configured bus handles, sensor population,
calibration and policy records. Optional motion evidence comes from an explicitly
provided source; there is no fabricated encoder data or new UART implementation.
The task refuses invalid startup configuration. Results are available through a
serialized snapshot; no logging or calibration writes occur inside the cycle.
Application-owned objects must outlive their tasks; runtime reconfiguration is not
provided. ISR/watchdog and recovery/ARM integration remain subsequent work.

The GPIO adapter remains **hard-inhibited**, and this worker supplies neither
recovery nor ARM authorization. There is no setting that enables physical permits.
The updated `build_check` target compile-links the full path but deliberately does
not start it: **it is not a firmware image to flash for a motor test**. Existing
sensor-health and UART diagnostics remain the available fixture applications.
A runnable calibrated operational application still requires accepted parameters,
board integration and the remaining independent fault paths.

## Tests and next work

Portable tests cover current persistence/hysteresis, low INA with high IPROPI,
stall proxy and motion validation, negative supply current, battery faults,
calibration corruption, clipping, population mismatch, missing INA, deadlines,
fault latching and the complete six-ADC/24-read workload. A replay at 570 us per
read intentionally fails the acquisition budget; the implementation does not
silently widen it. D10 supervisor tests hold permission across the old age cutoff,
then prove that a missing next cycle still inhibits at its work deadline.

Validation: **33/33 native software tests**, **5/5 portable ASan/UBSan tests**,
ARM cross-build/QEMU of the supervisor and policy tests, and the pinned ESP32-S3
compile/link harness pass. Evidence is stored under `docs/evidence/monitor_policy/`. Host and target builds
are not electrical acceptance. Prior physical diagnostic results remain scoped to
their original images and do not validate this new policy on target.

Next: select/document actual calibration and practical current/time thresholds;
measure or reduce full four-device bus work while preserving explicit checks;
integrate ISR/STOP/HALT/watchdog and deliberate recovery; verify ITRIP on assembled
hardware before bounded powered commissioning. Motor thermal survival modeling
is no longer a release blocker. G1a/G1b/G1c/G2 remain open, G3–G7 unaccepted.

Heartbeat-separation regression evidence: [results and hashes](evidence/heartbeat_separation/results.json), 33/33 native and 5/5 sanitizer tests, ARM/QEMU and ESP32 compile/link pass. No physical waveform acceptance is claimed.
