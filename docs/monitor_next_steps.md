# Monitor implementation follow-up — 2026-11-08

The first three implementation items in `system_status.md` now have code and host
validation. Operational deployment is still blocked by calibration, protection
policy, tighter conversion-age qualification, recovery integration and electrical
shutdown evidence. No PL pins, ESP32 pin assignments or UART frame layouts changed.
The PS control loop remains 1 ms. Commissioning-first release scope is unchanged.

## 1. Explain rejected acquisition replies

`dmotor-acquisition-test` now terminates immediately on a decoded unacceptable
reply and prints `FAIL Sample rejected: REASON`. Reasons distinguish envelope,
challenge, sequence, boot identity, cycle replay, expiry, skipped cycle, lifetime
failure count, sample status, read count, flags, mode, address, duration and age.
When several conditions fail, the first check wins (expiry precedes skipped,
failed and sample status). Malformed frames and silence still use the existing
bounded receive path. Counters and wire layout are unchanged.

This makes the earlier `mismatched=1` plus timeout results interpretable without
pretending they were missing UART replies. Historical excerpts are not rewritten.
The I/O-only target rerun now passes with explicit `failure` rejection; see
[paired output and interpretation](prepcb_acquisition.md#explicit-receiver-rejection-rerun--2026-11-08).
Delayed injection should report `expiry` once its lifetime expiry counter is nonzero;
that new-receiver reason has not been physically reported. Do not reset those counters to make an unhealthy run pass.

Build output: `build/monitor-next/arm/dmotor-acquisition-test`. Follow the existing
SCP/install procedure in [pre-PCB acquisition](prepcb_acquisition.md). The maintained
Yocto recipe includes the new portable test; no new WIC was generated here.

## 2. Physical sensor health and calibration schema

`physical_acquisition.hpp` reads each explicitly installed device once per cycle:
manufacturer, die ID, configuration, mask/enable, signed shunt and bus voltage.
Duplicate physical addresses are rejected. Missing chips never reduce the configured
population. Identity/configuration mismatch, incomplete transport and clock regression
invalidate readiness history. No sensor register is written by this implementation.

The first successful mask read establishes a clear baseline and cannot qualify data.
A later CVRF assertion proves a conversion completed since that clear. Raw value
changes alone are never treated as freshness. The oldest possible conversion-window
start is conservatively the preceding mask-read start minus a configured maximum
conversion/averaging duration. Data at or beyond 25 ms age is invalid. The bound
must at least cover the nominal duration implied by configuration bits; tolerance
and actual sensor behavior still require qualification. This follows TI's
[INA226 mask/enable and conversion-ready semantics](https://www.ti.com/lit/ds/symlink/ina226.pdf).

The calibration record carries version, generation, board identity, population,
ten fixed-point gains/offsets and a canonical little-endian CRC32. Channels 0–3
convert shunt counts to mA, 4–7 bus counts to mV, and 8–9 both battery ADCs to mV.
No fabricated calibration is installed. Missing required gains, wrong board or
population, unsupported version and corruption reject conversion. CRC detects
corruption, not unauthorized changes. Persistence, reviewed physical limits and
measured calibration coefficients remain implementation/acceptance work.

### Inhibited one-INA firmware

A separate diagnostic is under `software/esp32/diagnostics/sensor_health`. It uses
GPIO15 SDA / GPIO16 SCL, 100 kHz, address **0x44**, expected configuration **0x4127**,
and provisional conservative conversion bound **2,500 us**. These are explicit
menuconfig values; the bound is not a measured acceptance result. It performs six
reads per physical device rather than aliasing one device into four slots. It does
not sample ADCs or serve acquisition UART replies. Existing UART/timing profiles
remain available for their original tests.

From the dmotor repository:

```sh
../esp32-dev/esp32 --project software/esp32/diagnostics/sensor_health --target esp32s3 idf \
  -B "$PWD/build/sensor_health/esp32/build" \
  -D SDKCONFIG="$PWD/build/sensor_health/esp32/sdkconfig" \
  -D SDKCONFIG_DEFAULTS=sdkconfig.defaults build
```

Use the same command with `menuconfig` instead of `build` to edit the explicit
address/configuration/bound. Saved sdkconfig takes precedence over defaults.
With motor drive power disconnected, add `--port /dev/ttyACM0` before `idf` and
replace `build` with `flash monitor` when ready to test. Capture the startup ELF
hash, configuration and all output. No firmware was flashed during this change.

The diagnostic records 100 cycles at 16 ms, then prints
`health,cycle,status,duration_us,signed_shunt,bus,oldest_possible_us` and a summary.
Expect an initial `warming` result, then `good` if the device/configuration and age
checks succeed. Zero raw bus voltage is not a calibration or powered test result.
For missing-device/configuration tests, expect invalid health, never silent removal.
Do not compare this six-read workload directly with the prior twelve-read alias
benchmark. Four devices now require 24 reads: **the 12 ms work budget is not yet
proven for that path**, and may require a reviewed faster bus or a different
identity/configuration validation strategy. Do not widen the budget automatically.

## 3. Cadence, fault epochs and serialized safety decisions

The behavioral monitor simulator and joint recovery/coordinator tests now use
16 ms samples, ten healthy ARM checks (160 ms allocation) and maximum 16 ms
health/scheduling gaps. Debounce remains elapsed 20 ms. Recovery transport retries
remain 100 ms; those are not monitor periods. These are host models, not live recovery.

`safety_core.hpp` provides a portable candidate supervisor with cycle/epoch tokens,
12 ms work expiry, monotonic time, missed-cycle checks and a latched fault state.
Late or pre-fault results cannot reauthorize outputs. Final decisions require the
configured sensor mask, both battery proofs, calibration, policy, recovery and
physical ARM. Missing recovery/ARM alone leaves outputs inhibited. A completed, valid monitoring/policy evaluation toggles modeled HB independently
of ARM and operational fault latches; motor permission additionally requires a
clear policy and explicit authorization. Each such evaluation toggles HB once; the resulting nominal waveform is
31.25 Hz, which remains subject to PL watchdog compatibility qualification.

An independent `monitor_safety` FreeRTOS task observes the core every 1 ms at high
priority. All core operations and GPIO writes use the same short critical section;
I2C, logging and calibration do not run under that lock. Its GPIO adapter keeps
permit GPIO8–11 **hard-inhibited LOW** and writes qualified HB to GPIO21. There is
no setting that enables motor permits. The sensor diagnostic exercises this adapter but lacks the
battery/calibration/policy/recovery proofs, so it cannot qualify operational outputs.
The compile-check harness references the adapter without starting it.

The supervisor expires a previous permit decision if the next worker never
starts/completes by the next release+12 ms. D10 removes continuous conversion-age expiry. With the conservative age
estimate above, 25 ms validity can end before the next 16 ms cycle completes.
That was the pre-D10 policy. D10 explicitly accepts motor-damage risk and retains
age checking at commit, with independent next-cycle liveness instead.

The task is independently scheduled, not an interrupt-level deadline guarantee.
STOP/HALT ISR integration, watchdog behavior under scheduler starvation, measured
low-going electrical latency, accepted threshold policy and a full operational
application remain open. Fault latches have no automatic/live rearm API. Keep
G1a/G1c and G3–G5 open; no physical qualification follows from these host tests.

## Verification and next fixture work

- Native software: **32/32 tests passed**, including expanded PTY rejection tests.
- Portable acquisition/sensor/safety: **4/4 ASan/UBSan tests passed**.
- ARM cross-build and QEMU: receiver help, acquisition codec and sensor/safety tests passed.
- ESP32-S3 pinned ESP-IDF build: physical sensor-health diagnostic passed.
- Logs, build commands and source/artifact hashes: [evidence](evidence/monitor_next_steps/results.json).

Next, extend the passing
physical-health capture and validate its conversion bound against the analyzer, then
perform known-reference battery calibration. Keep the assembled four-device timing,
shutdown latency and recovery/ARM gates open until their respective hardware exists.

## Physical-health release scheduling correction — 2026-11-08

The [reported first physical-health run](evidence/sensor_health_release_fix/reported-run.txt)
failed: cycle 0 was `warming` after 4,100 us, and cycles 1–99 were `io` with
negative release-to-end durations (-537/-539 us). This is a diagnostic scheduling
bug, not evidence of an INA NACK: tick-based `vTaskDelayUntil` woke before the
independently captured microsecond release. The acquisition guard rejected work
before the first bus transaction and the old result path labeled it `io`.
Cycle 0 passed transport, identity and configuration checks and established a
CVRF clear baseline; no subsequent fresh conversion was qualified. No gate closes.

The diagnostic now waits against the microsecond clock before supervisor begin
and acquisition, rechecking after tick wakeups and waiting only the sub-tick tail
with a microsecond delay. Absolute 16 ms releases are preserved; late wakeups are
not hidden by rebasing. A portable regression reproduces a 539 us early wake,
repeated early returns, backwards time and late arrival. The original timing
budget, I2C settings and hard-inhibited outputs are unchanged.

The corrected image passed the user-reported 100-cycle physical rerun below.
The original failed run remains preserved; it is not sensor-freshness evidence.
No flashing was performed by the coding agent.

## Passing one-INA physical-health rerun — 2026-11-08

**PASS for the inhibited one-device diagnostic**, based on the user-supplied
[capture excerpt](evidence/sensor_health_release_fix/passing-rerun-excerpt.txt).
Reported ELF prefix `f6ac4ce3a`, address `0x44`, configuration `0x4127`, configured
conversion-window bound 2,500 us. The prefix alone is not a full firmware identity.

| Observation | Result |
|---|---|
| Warm-up baseline | Cycle 0, health=4, 3,918 us; expected |
| Qualified samples under configured checks | Cycles 1–99, 99/99 health=0 |
| Post-warm-up release-to-end duration | 3,484–3,539 us |
| Maximum including warm-up | 3,918 us |
| I/O/config/identity/not-ready/stale/overflow/clock errors | None reported |
| Negative durations | None |
| Raw signed shunt / bus counts | -4 to -2 / 0 to 2; not calibrated units |

This validates the release correction on the current fixture and repeated
transport, identity/configuration, CVRF-history and computed age checks for one
INA. The 2,500 us conversion bound remains a configuration assumption requiring
independent validation; these logs do not measure conversion latency directly.
The final CSV column is an oldest-possible conversion timestamp, not sample age.

The 3,918 us maximum leaves 7,082 us before the 11 ms acquisition ceiling for
this one-device, six-read, no-ADC workload. It does not qualify the four-device,
24-read workload or the complete 12 ms monitor work budget. The 16 ms period and
25% loop reserve requirement are unchanged. Console `permits/HB LOW` remains a
software report, not electrical shutdown evidence. G1c/G2 and G3–G5 remain open.

Next fixture validation: collect a longer
physical-health run and analyzer evidence for conversion timing. Known-reference
ADC calibration and operational policy/recovery/shutdown integration remain open.

## Wrong-address negative test — 2026-11-08

**PASS for rejection at an unresponsive configured address**, assuming the physical
INA remains at 0x44 and only the diagnostic address was changed to 0x43.
[User-supplied capture](evidence/sensor_health_release_fix/wrong-address-run.txt):
ELF prefix `86793f280`, expected configuration `0x4127`, bound 2,500 us.
All 100 cycles report health=3 (`io`); zero samples qualify, all raw outputs and
oldest-conversion timestamps are zero. Maximum release-to-end is 739 us; cycles
1–99 range from 280 to 312 us. No negative durations appear.

This verifies rejection rather than discovery/fallback to the responsive 0x44
sensor. The log does not distinguish address NACK from other transport failures,
and does not test an identity or configuration-register mismatch on a responding
device. Software reports permits/HB LOW; no electrical shutdown latency is proven.
The successful-address baseline and this negative subcase advance G2 evidence;
G1c/G2 and operational protection gates remain open.

Next, restore the diagnostic address to 0x44 and confirm the nominal result.
For the separate configuration-check test, change only the diagnostic's expected
configuration from 0x4127 to 0x411f while leaving the actual INA register unchanged.
The diagnostic is read-only and should report health=1 (`config`), good=0.
Use 0x411f because it preserves the required low mode bits (`& 7 == 7`)
and its nominal conversion duration fits the 2,500 us bound. Values 0x4125
and 0x4126 fail local configuration validation at startup and cause a reboot
loop; they do not exercise the sensor register-mismatch check.
Restore the expected value to 0x4127 afterward. This is distinct from physically
changing the sensor configuration. The operator reports this step was already
completed in another chat; its raw result still needs linking. The I/O-only UART
rerun below has now passed, so neither step is requested again.

## Current disposition and remaining sequence

Nominal physical health, wrong-address rejection and explicit I/O-only UART
rejection have passed. Expected-register mismatch is operator-reported completed
in another chat; its capture is not available here, so detailed evidence linkage
remains pending without requiring a repeat.
The next implementation/validation priorities are maintained in
[system status](system_status.md#next-implementation-and-validation), with the
consolidated acceptance/evidence matrix in [G2](gates/G2.md).

**Procedure correction:** The previously suggested expected value 0x4126 is
invalid for this diagnostic's required continuous shunt+bus mode and aborts at
startup validation. Use **0x411f** for the register-mismatch test: it passes the
configuration validator with the existing 2,500 us bound, but differs from the
unchanged sensor's 0x4127. The operator reports completion elsewhere; the raw
capture has not been linked here.

## Explicit I/O-only UART rejection — validated

[Paired Zynq/ESP32 result](prepcb_acquisition.md#explicit-receiver-rejection-rerun--2026-11-08):
Zynq rejects the first decoded reply with `Sample rejected: failure`;
ESP32 records 9,600 complete and 400 failed cycles, no skips/expirations,
maximum 5,747 us. This completes the requested updated-receiver negative test.

Configurable long physical-health capture with release, conversion-age and
bus-error telemetry is implemented and its nominal physical run passed. Validate the
conversion-age bound, then resolve the four-device 24-read workload and complete
calibration/policy integration. Preserve the 16 ms period, 12 ms work ceiling,
4 ms reserve and hard-inhibited outputs. Physical qualification gates remain open.

## Long physical-health capture — nominal physical run passed

The one-INA diagnostic now supports `DMOTOR_HEALTH_CYCLES` from 100 to 100,000,
default **10,000 cycles (about 160 seconds)**. RAM remains fixed: aggregate counts
cover the entire run; the last 100 records and first unexpected failure are retained.
There is no console formatting during acquisition. The initial cycle's warming
status is expected and does not become `first_bad`. Later warming is retained.

Use a separate build/config directory so previous mismatch-test settings do not
carry into the nominal capture. From the dmotor repository:

```sh
../esp32-dev/esp32 \
  --project software/esp32/diagnostics/sensor_health \
  --target esp32s3 --port /dev/ttyACM0 idf \
  -B "$PWD/build/sensor_health_long/esp32/build" \
  -D SDKCONFIG="$PWD/build/sensor_health_long/esp32/sdkconfig" \
  -D SDKCONFIG_DEFAULTS=sdkconfig.defaults \
  build flash monitor
```

Confirm `address=0x44 config=0x4127 conversion_bound_us=2500` and
`capture_v2 cycles=10000 period_us=16000 retained_tail=100` at startup.
The console then stays quiet during capture. Keep motor drive power disconnected.
**No Zynq application is required**; this profile has no acquisition UART server.
Use the same host command with `menuconfig` instead of `build flash monitor` to
change cycle count. Saved config overrides defaults; verify the banner every time.

CSV v2 rows contain `kind,cycle,health,release_us,lateness_us,duration_us,age_upper_us,
shunt,bus,attempts,esp_error,error_reg,transport_complete`. The old CSV layout is
superseded only for this standalone diagnostic; no operational UART contract changes.

- Age is a conservative upper bound at acquisition return, using the configured
  conversion-window bound; `-1` means unqualified, not a fresh reading.
- `esp_error` is the IDF return code of the first failed bus call that cycle;
  `error_reg` identifies the register. A code alone does not prove electrical cause.
- `guarded_cycles` counts incomplete acquisition without an actual bus error
  (e.g. deadline/clock guard); it is distinct from `bus_error_cycles`.
- `late_releases` counts start lateness >=1,000 us; `acquisition_deadline_misses`
  counts duration >=11,000 us. Overdue releases are not rebased or silently skipped.
- All nine health-status totals, maxima and first-failure evidence survive tail
  buffer rollover. No samples or records are streamed during the timed loop.

Expected nominal result: one warming, **9,999 good**, no other health statuses,
no bus-error/guarded cycles, no late releases or acquisition deadline misses.
Qualified age bounds must stay below 25,000 us. Report the full startup banner,
status totals, summary and any `first_bad` row. Do not interpret successful snapshots
as a guarantee that a permit can remain valid until the next cycle.

Portable sanitizer tests pass 4/4, including rollover over 10,000 records, first
failure retention and timing/error counts. ESP32 target build evidence is under
`docs/evidence/sensor_health_long/`. The nominal physical run passed as recorded below.
Next validation is analyzer confirmation of conversion timing and stale-readable rejection;
four-device timing, calibration and operational safety gates remain open.

### Validated 10,000-cycle result

[Unmodified user capture](evidence/sensor_health_long/physical-run.txt), reported
ELF prefix `45d41942d`, address 0x44, expected configuration 0x4127, assumed
conversion bound 2,500 us: **PASS for the nominal one-INA long-run diagnostic**.

| Measurement | Whole-run result |
|---|---|
| Cycles / qualified / warming | 10,000 / 9,999 / 1 |
| Other health statuses | All zero |
| Maximum release-to-end | 3,905 us |
| Maximum release lateness | 24 us |
| Maximum qualified age upper bound | 20,240 us |
| Bus errors / guard rejections | 0 / 0 |
| Late releases (>=1 ms) / acquisition deadline misses | 0 / 0 |

The retained tail covers cycles 9900–9999, all good with six successful reads,
16,000 us between scheduled releases, 5 us lateness, 3,533 us duration and
20,239 us age bound. Counts/maxima cover the whole run; raw rows cover only the
tail. No `first_bad` appears. The application-return timestamp is not an exact
capture duration; the nominal schedule spans about 160 seconds.

The maximum leaves 7,095 us before the 11 ms acquisition ceiling for this
single-device workload. The age bound leaves only **4,760 us** before the 25 ms
freshness limit at the worst observed qualified sample, less than the 16 ms
inter-cycle spacing. Thus this is a snapshot-health pass, not sustained permit
validity. Do not relax the age limit to conceal that gap. The 2,500 us sensor
conversion bound still requires independent evidence; this telemetry is calculated,
not a direct conversion-latency measurement.

The supplied ELF prefix is not a full image hash. Software-reported LOW does not
measure electrical outputs. No four-device/ADC/policy timing or operational
protection acceptance follows. G1c/G2 and G3–G5 remain open. Next work: validate
conversion timing/clear semantics with the analyzer and stale-but-readable tests,
then address the tighter freshness proof and 24-read four-device timing strategy.

## Adopted best-effort current protection — D10

The historical freshness-window findings above remain measurements of the earlier
policy. [D10](decisions.md#d10--best-effort-motor-overload-protection-and-accepted-motor-damage-risk)
now accepts motor damage from delayed monitoring/shutdown, conditional on verified
ITRIP. The 25 ms check applies at snapshot commit, not continuously between cycles;
next release+12 ms liveness, invalid-data inhibition and fault epochs remain.
Steps 1 and 2 of that decision are implemented as the [complete workload/policy
candidate](monitor_policy.md). Physical motor permits remain hard-inhibited;
qualified heartbeat is now emitted independently. Calibration,
actual thresholds, four-device timing and ISR/recovery integration remain open;
motor-survival deadline proof is no longer a release blocker. No new hardware run
or gate acceptance is claimed.

## Heartbeat/permission implementation correction

The candidate supervisor and policy pipeline now separate monitoring health from
motor permission, matching the recovery contract. Healthy monitoring while
SAFE/disarmed or FAULT-latched advances modeled HB; failed monitoring stops HB.
Resumed HB never clears a motor-fault latch or grants permission. See the
[truth table and implementation semantics](monitor_policy.md#heartbeat-is-independent-of-motor-permission).
The GPIO adapter now emits qualified HB on GPIO21; motor permits remain
hard-inhibited. The one-INA diagnostic still lacks the full monitoring proof and
therefore keeps HB LOW. See [board heartbeat validation](board_heartbeat.md) for
the U1 test, SW0 interlock and LD4/LD5 indicators. The operator has confirmed
synthetic-HB blue indication, loss red indication and SW0-asserted LD4 red
([record](evidence/board_heartbeat/physical_confirmation_2026_11_08.md)).
Next fixture work is secure connection, waveform/timing capture and measured
SW0 output/latch/ACK checks; operational sensor-qualified HB is still open.
