# Pre-PCB acquisition and UART integration diagnostic

Implemented 2026-11-08 for the existing Zynq + ESP32-S3 + UART + one INA fixture.
This extends the [100/100 standalone result](ina_bench_timing.md). **The concurrent
acquisition/UART transport baseline now has an operator-reported physical PASS**. Motor power stays
disconnected and GPIO8/9/10/11 permits plus GPIO21 HB remain commanded LOW throughout.
No ARM, operational recovery, calibrated measurements or healthy-HB path exists.
No PL, pinout, daemon or client API changes. The diagnostic tty utility owns only
the UART; it must not run alongside the daemon or another tty owner. Any future
application access to the daemon still uses `dmotor_client`.

## Physical transport result — 2026-11-08

[Supplied Zynq and ESP32 excerpts](evidence/prepcb_acquisition/physical_run_2026_11_08.txt):

- Zynq: **1,000/1,000 replies verified**, rejected=0, mismatched=0, elapsed 70.009 s.
- ESP32: **10,000/10,000 acquisitions complete**, failed=0, skipped=0, observer
  expirations=0, nominal 160 s acquisition run.
- Whole-run maxima: release-to-completion **8.215 ms**, INA **7.874 ms**, ADC
  **0.316 ms**, release lateness **0.577 ms**. The separate `# summary` covers
  only the last 100-cycle batch; use `# aggregate` for whole-run conclusions.
- Acquisition leaves **3.785 ms** before the 12 ms work deadline and **7.785 ms**
  before the next 16 ms release (48.66% of the period). INA headroom is 2.126 ms.
  These are observed margins without operational policy/output processing, not WCET.

Address 68 is decimal **0x44**. The varying raw INA/ADC data are consistent with
real-input operation on the session's one-INA fixture; startup mode/configuration
headers, command invocation and flashed-image hashes were not included. Retain
that provenance limitation rather than assigning a local build hash retrospectively.
The UART run covered about 70 s of the longer acquisition run; do not claim UART
load was present for all 160 s. Nonconsecutive receiver cycle numbers are expected
because it polls more slowly than acquisition; they are not skipped acquisitions
(the aggregate explicitly reports zero skipped releases). `remote_age_us` is age
since acquisition completion, not the age of the physical conversion.

This passes the concurrent transport baseline for the supplied run. The zero
observer count validates only the nominal path, not deadline-fault response.
The delayed-failure result is recorded below; synthetic nominal, four physical sensors,
freshness/calibration and output/protection qualification remain outstanding.
Permits/HB LOW is a firmware report, not an electrical measurement.

## Delayed-failure result — 2026-11-08

The [operator-supplied excerpts](evidence/prepcb_acquisition/delayed_run_2026_11_08.txt)
meet the delayed-injection diagnostic criteria:

| Whole-run result | Observed |
|---|---:|
| Cycles / complete / failed | 10,000 / 9,600 / 400 |
| Skipped releases | 400 |
| Independent observer expirations | 400 |
| Maximum release-to-end / INA window | 23.269 / 23.200 ms |
| Maximum ADC duration / release lateness | 0.048 / 0.135 ms |

An injection every 25 cycles predicts 10,000 / 25 = 400 failures, matching the
observed count. Each >16 ms acquisition consumes the next release; the observed
400 skipped releases are consistent with skipping rather than catching up.
These overruns are deliberate negative-test evidence, not normal budget passes.

Both Zynq invocations sent one request and received one decoded but unacceptable
reply (`mismatched=1`, `rejected=0`, `verified=0`). The receiver increments mismatch
but continues waiting for an acceptable reply, hence the eventual generic
`Response/TX timeout`. This does not indicate an absent UART reply. The excerpts
do not identify which Tracker predicate rejected it; lifetime failure/skip/expiry
counters are sufficient to reject any later snapshot after the first injection,
but raw reply fields were not supplied. End-of-run stale data could also reject.
Restarting the receiver does not clear ESP32 lifetime counters. Reset the ESP32
and use the normal profile for another nominal run.

This is a **PASS for injected-overrun detection and observed receiver rejection**.
Counts do not measure observer callback latency or electrical inhibit response;
there is no timestamped fault trace or output transition measurement. Permits/HB
were always commanded LOW. Startup headers, full cycle logs and image hashes
were not supplied. The I/O-only result is recorded below; conversion freshness,
calibration, assembled loading and operational protection/recovery remain open.

## Reboot and UART-loss results — 2026-11-08

[Operator-supplied Zynq excerpts](evidence/prepcb_acquisition/link_loss_2026_11_08.txt)
show accepted samples followed by receiver failure:

| Operator action | Sent | Verified | Rejected / mismatched | Result |
|---|---:|---:|---|---|
| ESP32 reboot | 45 | 44 | 0 / 0 | Response/TX timeout |
| UART disconnected | 34 | 33 | 0 / 0 | Response/TX timeout |
| RX-side test (operator label: “With the RX”) | 72 | 71 | 0 / 0 | Response/TX timeout |

All three pass the **diagnostic lost-reply detection** criterion: after successful
communication, the next request was sent but no acceptable response completed.
No decoded mismatch was reported, unlike the delayed-injection test. The exact
wire/direction for the RX-labelled test was not specified and is not inferred.
The reboot test demonstrates timeout on the reboot interruption, not explicit
rejection of a received changed boot ID. That remains covered by automated tests,
not established by these hardware excerpts. No measured fault-to-exit latency or
electrical shutdown timing was supplied; the receiver uses a configured 1 s
request/TX deadline, not the monitor's 12 ms work deadline.

The subsequently supplied [restored-link excerpt](evidence/prepcb_acquisition/restored_link_2026_11_08.txt)
reports **PASS: sent=1000, verified=1000, rejected=0, mismatched=0**, elapsed
69.999 s. This confirms successful communication in an explicit new invocation
after the preceding interruption tests. The final excerpt omits the run nonce,
so association with the prior startup is based on operator conversation context,
not an independently matched identifier. No new ESP32 aggregate was supplied.
This is diagnostic link restoration, not automatic/live recovery or motor
protection qualification. I/O-only injection subsequently passed as recorded below;
retain full headers and identities in future captures.

## I/O-only injection result — 2026-11-08

[Operator-supplied excerpts](evidence/prepcb_acquisition/io_failure_2026_11_08.txt)
meet the I/O-only diagnostic criteria:

| Whole-run result | Observed |
|---|---:|
| Acquisitions / complete / failed | 10,000 / 9,600 / 400 |
| Skipped releases / observer expirations | 0 / 0 |
| Maximum release-to-end / INA window | 5.746 / 5.566 ms |
| Maximum ADC duration / release lateness | 0.046 / 0.134 ms |

Exactly 400 failures match injection every 25 cycles. The supplied failed read
is ordinal 6, logical slot 2, register 0x06, duration 1 us, error -1 (injected
ESP_FAIL), raw 0 invalid. This is software-injected failure, not a measured I2C
NACK from the real INA. Unlike the delayed case, the owner rejects the cycle
without overrunning: no skipped releases or observer expirations are reported.

Both Zynq invocations received one decoded but unacceptable reply, then exited
with the generic timeout (`sent=1 verified=0 rejected=0 mismatched=1`). The receiver
retains its existing behavior: rejected sample acceptance does not end its wait
immediately. Lifetime failure count is sufficient to reject later snapshots, but
the exact predicate cannot be identified without the rejected payload. Starting
a new receiver invocation does not clear the ESP32's lifetime failure count.

**PASS for software-injected I/O failure handling and observed receiver rejection.**
The last-batch summary is distinct from the full-run aggregate. Startup header,
complete CSV and image hashes were not supplied; configuration is inferred from
the test sequence and the matching injected-read/count behavior. Commanded-low
outputs are not an electrical measurement. This result does not qualify real
bus-stall recovery, conversion freshness, calibration or operational protection.

The reported pre-PCB transport sequence now covers nominal concurrent acquisition,
delayed failure, reboot/UART loss, explicit link restoration and I/O-only failure.
Next implementation priorities: clearer receiver rejection reasons; physical-device
conversion readiness/age tracking and ADC calibration; 16 ms recovery-model and
operational safety-task integration. Four-device and electrical shutdown validation
still await the assembled fixture. No automatic motor recovery is implemented.

## What is implemented

- The existing `ina_timing` application supports finite consecutive batches:
  default new profile = 100 batches × 100 cycles = 10,000 cycles, nominal 160 s.
  One batch buffer is reused. No console dumps interrupt acquisition between
  batches; final output contains detailed last-batch rows and lifetime aggregate
  completions/failures/skips and duration maxima. Buffer memory does not grow with
  batch count. Earlier bounded/raw profiles default to one batch.
- UART1 TX17/RX18 remains 115200 8N1. A separate task answers snapshot requests
  during acquisition. The receiver polls at no more than 20 Hz. The acquisition
  task never waits for UART transmission or holds a snapshot lock during I2C.
- Header reports ELF SHA256, build version/time and test configuration. This ELF
  identity is distinct from the `.bin` SHA256; retain both with each hardware run.
- Real-input initialization scans 0x40–0x4F (prefers 0x44), reads manufacturer ID
  0xFE, die ID 0xFF and configuration 0x00. The new UART profile requires 0x5449 and
  die family 0x226x; no registers/thresholds are written. See the
  [TI register definitions](https://www.ti.com/lit/ds/symlink/ina226.pdf).
- Synthetic mode provides clearly flagged fake INA/ADC values, allowing UART/load
  testing without sensors. The receiver requires `--synthetic` for those replies
  and rejects synthetic replies in a real-data run (and vice versa).
- Software injection can fail read 6 (third logical slot) or delay it by 20 ms
  every Nth cycle. The independent `esp_timer` task observes the 12 ms work
  deadline, records expiry and reasserts commanded-low outputs. Generation checks
  and a critical section prevent late publication from undoing expiry. This is
  task-level diagnostic supervision, not a qualified hard-real-time shutdown.
  Outputs never rise, so electrical high-to-low shutdown cannot be measured here.

## Explicit receiver rejection rerun — 2026-11-08

**PASS for I/O-only negative-test criteria**, from [the supplied paired output](evidence/prepcb_acquisition/explicit_rejection_run_2026_11_08.txt).
Zynq run `13832978125558781453` reports `FAIL Sample rejected: failure` on
its first request: sent=1, verified=0, rejected=0, mismatched=1. The decoded
reply was refused because its lifetime failed-acquisition counter was nonzero;
`rejected=0` refers to decoder rejection, not semantic acceptance. This is the
expected negative result and replaces the earlier ambiguous timeout wording for
this receiver version. It does not measure rejection latency.

ESP32 aggregate: 10,000 cycles, 9,600 complete, **400 failed**, zero skipped and
zero deadline-observer expirations. Maximum release-to-end **5,747 us**, INA
window 5,566 us, ADC 46 us and lateness 135 us. The final 100-cycle batch has
96 complete and maximum 5,558 us. These match the configured every-25-cycle
I/O injection; this synthetic workload is not a real INA timing measurement.
One rejected UART request is not coverage of every injected event over UART.

Startup/config/image hashes were not included in this capture. Commanded LOW is
not electrical shutdown evidence. G2's subcase advances; G1c/G2 and operational
protection gates remain open. No automatic recovery is demonstrated.

## Build and run

From dmotor with the pinned `esp32-dev` checkout beside it:

```sh
PROFILE=prepcb # alternatives: prepcb_synthetic, prepcb_fault
../esp32-dev/esp32 --project software/esp32/diagnostics/ina_timing \
  --target esp32s3 idf \
  -B "$PWD/build/$PROFILE/esp32/build" \
  -D SDKCONFIG="$PWD/build/$PROFILE/esp32/sdkconfig" \
  -D SDKCONFIG_DEFAULTS="sdkconfig.$PROFILE.defaults" build
```

To flash, repeat with `--port /dev/ttyACM0` before `idf` and replace the final
`build` with `flash monitor`. The three profiles use separate build/config paths.
`prepcb_fault` is the ESP32 profile for **both** delayed-failure and I/O-only
injection. It uses synthetic inputs and injects every 25 cycles. Select the
failure type in that profile's saved configuration:

- `DMOTOR_INJECT_DELAY=y`: inject a 20 ms delay (the delayed test).
- `DMOTOR_INJECT_DELAY` disabled: inject an I/O failure without that delay.

### Select I/O-only injection using menuconfig

Run on the **development host**, from the dmotor repository root. `menuconfig`
edits build configuration on the host; it does not run on or flash the ESP32.

```sh
cd /home/rklinkhammer/workspace/dmotor
../esp32-dev/esp32 \
  --project software/esp32/diagnostics/ina_timing --target esp32s3 idf \
  -B "$PWD/build/prepcb_fault/esp32/build" \
  -D SDKCONFIG="$PWD/build/prepcb_fault/esp32/sdkconfig" \
  -D SDKCONFIG_DEFAULTS=sdkconfig.prepcb_fault.defaults \
  menuconfig
```

Open **DMOTOR isolated INA / ADC timing**, then disable
**Inject a 20 ms delay instead of an I/O failure**. Leave synthetic inputs and
acquisition UART enabled, and **Inject at every Nth cycle** at **25**. Save to the
selected sdkconfig and exit. The saved file should contain
`# CONFIG_DMOTOR_INJECT_DELAY is not set`.

Build and flash that same configuration:

```sh
../esp32-dev/esp32 \
  --project software/esp32/diagnostics/ina_timing \
  --target esp32s3 --port /dev/ttyACM0 idf \
  -B "$PWD/build/prepcb_fault/esp32/build" \
  -D SDKCONFIG="$PWD/build/prepcb_fault/esp32/sdkconfig" \
  -D SDKCONFIG_DEFAULTS=sdkconfig.prepcb_fault.defaults \
  build flash monitor
```

Confirm the ESP32 startup header reports `synthetic=1 inject_every=25 inject_delay=0`.
Then, during the acquisition run, invoke on Zynq:

```sh
sudo ~/dmotor-acquisition-test /dev/ttyUL0 100 --synthetic
```

Use the actual serial devices on each host. Use the newly copied `~/dmotor-acquisition-test` so an older installed binary
does not hide the rejection reason. Receiver failure is expected after
an injected error: `FAIL Sample rejected: failure` for I/O-only injection, or
`FAIL Sample rejected: expiry` for delayed injection once expiry is recorded. With no unrelated faults, expect 400 failed and 9,600 complete
acquisitions over 10,000 cycles; skips and deadline expirations should remain zero
because this test injects no delay. Retain the actual aggregate and observer counts.

Existing sdkconfig overrides defaults. The tracked `prepcb_fault` defaults still
select delayed injection for a fresh build directory. To repeat the delayed test,
re-enable the option in menuconfig, rebuild and flash. Do not use `prepcb` or
`prepcb_synthetic` for this injection procedure.

The Linux receiver builds/installs as `dmotor-acquisition-test` through
`software/CMakeLists.txt`. Native build:

```sh
cmake -S software -B build/prepcb-native
cmake --build build/prepcb-native -j4
ctest --test-dir build/prepcb-native --output-on-failure
```

A Zynq ARM binary is at `build/monitor-next/arm/dmotor-acquisition-test`, cross-compiled
against the retained EDF Arty sysroot. It is not deployed. EDF source staging and `dmotor-runtime` packaging now include
the receiver; rebuild/export the Linux image to include it in a new SD image. For future application-only deployment, copy the ARM binary over the network
(the current full-image procedure remains supported):

```sh
scp build/monitor-next/arm/dmotor-acquisition-test USER@ZYNQ:~/dmotor-acquisition-test
ssh USER@ZYNQ
chmod +x ~/dmotor-acquisition-test
```

Replace `USER@ZYNQ` with the actual account and address. Run with an account that
has access to the selected UART; the binary must match the target runtime libraries.
This copies a standalone diagnostic, not a new filesystem or firmware image. Then run:

```sh
./dmotor-acquisition-test /dev/ttyUL0 100
# For prepcb_synthetic or prepcb_fault:
./dmotor-acquisition-test /dev/ttyUL0 100 --synthetic
```

Select the actual UART Lite device by its binding (0x42C00000); `/dev/ttyUL0` is
an example, not a guarantee. Start the receiver after ESP32 `READY` and the first
completed cycle, during its 160 s run. Opening the native USB monitor can reset
the ESP32; retain console output from startup. The receiver fails on initial
invalid samples, end-of-run staleness or restart; start a new invocation explicitly.
It does not reconnect/rearm automatically. Counts are 1–1000 per invocation.

## Diagnostic wire contract

Same bounded delimiter/escaping/CRC16 machinery as U0, but **magic 0xD8**, version 1
instead of U0's 0xD7. Operational recovery framing is not used or modified. Requests
are kind 1, payload length zero, random host run nonce and increasing request
sequence. Replies are kind 2, echo the challenge and contain exactly 64 bytes:

| Offset | Little-endian payload |
|---|---|
| 0 | u64 random ESP boot nonce (diagnostic, not durable recovery identity) |
| 8,12,16 | u32 cycle sequence, acquisition duration us, age since completion us |
| 20,24,28 | u32 lifetime failed cycles, skipped releases, observer expirations |
| 32–35 | u8 status (0 = complete), INA read count, address, flags |
| 36–59 | twelve u16 raw INA values (four logical slots × mask/shunt/bus) |
| 60,62 | first raw u16 ADC sample for each battery channel |

Flags: bit 0 injected cycle; bit 1 synthetic input; remaining bits reserved zero.
On failed acquisition raw payload is zeroed; consumers must check status. Receiver
requires a new cycle, unchanged boot ID, matching challenge, all 12 reads, no
lifetime failures/skips/expirations, and correct synthetic mode. It rejects remote
sample age >50 ms, request RTT >100 ms or age+RTT >100 ms. These are **diagnostic
transport recency limits**, not the operational 25 ms conversion-age requirement.
The clocks are not synchronized or subtracted across devices. Snapshot time is not
sensor conversion time. Mask/Enable reads can clear flags; four logical reads of
one INA cannot establish four sensors' independent freshness.

## Validation sequence and pass criteria

1. Synthetic profile: receiver verifies 100 advancing replies; aggregate has no
   failures/skips/expirations. Repeat under UART traffic while retaining full output.
2. Real profile: ID/config read succeeds, address matches fixture; 10,000/10,000
   acquisitions complete, zero skips/expirations, ADC <1 ms and INA <10 ms.
   Acquisition max must leave room for the absent policy/output phase before the
   <12 ms work deadline. This cannot qualify that phase or assembled loading.
3. Fault profile: receiver exits nonzero; aggregate shows injected failures and
   deadline-observer expirations. I/O-only injection must fail without requiring
   expiry. Recovery in later diagnostic cycles must not erase lifetime failures.
4. During a good run, reset ESP32 or disconnect UART: receiver must fail on boot
   change, stale data, tty error or timeout. Restore the fixture and start a new
   explicit run; no previously good sample can authorize anything.
5. Observe GPIO permits/HB low with the analyzer if connected. This is distinct
   from firmware's commanded-low statements and from PCB DRVOFF qualification.

Native automated coverage includes CRC/fragmented frames, wrong challenge,
missing replies, restart, repeated/old/failed samples, strict legacy U0 separation,
exact-deadline expiry and late-generation publication. ARM/QEMU checks establish
binary execution and portable logic only. The nominal concurrent run is recorded above; the delayed-failure run is also recorded above; other negative tests remain outstanding.

Next implementation: actual conversion-age tracking and ADC calibration,
production per-device handles/population, 16 ms recovery-model cadence, and full
operational safety-task/output integration. Those remain inhibited development;
this diagnostic does not implement live recovery or change commissioning scope.

## Recorded verification

[Build manifest and hashes](evidence/prepcb_acquisition/results.json),
[native test log](evidence/prepcb_acquisition/native-tests.log), and
[ARM/QEMU log](evidence/prepcb_acquisition/arm-qemu.log) record 31/31 native tests,
five ESP32 profile builds, ARM receiver compilation/help execution and the portable
link/deadline tests on ARM. The [ARM compile script](evidence/prepcb_acquisition/compile-arm.sh)
uses the existing EDF container/sysroot mounted at its recorded path and dmotor
mounted at `/work`; it does not rebuild or modify the SD image. The script is
specific to this retained toolchain. Snapshot hashes describe local artifacts,
not devices. Nominal concurrent physical evidence is recorded above; delayed-failure evidence is also recorded above; other negative tests remain outstanding.

### EDF staging/package correction

The first full EDF attempt failed at CMake configure because the new receiver,
link test, shared header and Python PTY test were omitted from `software/sources.json`.
They are now staged; `dmotor-runtime` owns `/usr/bin/dmotor-acquisition-test`, and
`do_semantics` also runs the link test on ARM QEMU. The corrected recipe passed
configuration, compilation, semantics and package QA (all 1,034 tasks succeeded,
1,014 reused): [recipe log](evidence/prepcb_acquisition/yocto-recipe.log).
This was recipe-only validation, not a new image/export. The installed hardware
fingerprint predates the staging edits; use the full EDF `build` command to produce
a new matched bundle/image. The saved fingerprint was not altered to bypass it.

## Implementation follow-up — 2026-11-08

See [receiver rejection reporting, physical sensor health and 16 ms safety candidate](monitor_next_steps.md).
Host tests and inhibited firmware builds advance implementation evidence only.
Existing hardware results remain tied to their original diagnostic images;
operational protection, electrical acceptance and live recovery are not claimed.

## Physical INA health diagnostic result — 2026-11-08

The separate inhibited physical-health firmware passed its corrected 100-cycle
run: one expected warm-up plus 99 good samples, maximum 3,918 us, no negative
durations or health errors. See [capture and scope](monitor_next_steps.md#passing-one-ina-physical-health-rerun--2026-11-08).
This six-read, one-INA test has no acquisition UART service or ADC workload and
does not replace the concurrent UART/ADC timing evidence above. The configured
conversion bound and electrical outputs remain independently unqualified.

**Wrong-address physical-health negative subcase (2026-11-08): PASS.** With the
configured address changed to 0x43 (physical INA assumed unchanged at 0x44), all
100 cycles reject with `io`, good=0, maximum 739 us; no fallback or negative
durations. [Capture and remaining tests](monitor_next_steps.md#wrong-address-negative-test--2026-11-08). This does not establish
register-configuration mismatch handling or an electrical shutdown result.
Overall gates remain open.

**Long one-INA physical-health result:** [10,000-cycle nominal capture](evidence/sensor_health_long/physical-run.txt)
passes with 9,999 good and one warming, max 3,905 us, lateness 24 us, calculated
age bound 20,240 us, and zero bus/guard/late/deadline counts. This advances U03/U06
and G2 diagnostic evidence only. The 4,760 us remaining age validity does not
cover another 16 ms cycle; conversion-bound proof, sustained output qualification,
four-device timing, calibration and electrical gates remain open. See
[next work](monitor_next_steps.md#validated-10000-cycle-result).
