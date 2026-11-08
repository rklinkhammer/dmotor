# Isolated ESP32-S3 INA226 + battery ADC timing test

**Pre-PCB implementation update (2026-11-08):** [Concurrent acquisition/UART diagnostic](../../../../docs/prepcb_acquisition.md)
adds fixed-memory long runs, a Zynq tty receiver, synthetic/error-injection profiles,
and task-level deadline observation. Existing standalone 100/100 physical evidence
remains valid for its original image. The concurrent run now reports 10,000/10,000
acquisitions and 1,000/1,000 Zynq replies, no failures/skips/expirations, max
acquisition 8.215 ms. Delayed injection also passed its diagnostic criteria: 400 expected failures,
400 skipped releases and 400 observer expirations; Zynq rejected the supplied
snapshots. Reboot and two UART-loss cases now show expected receiver timeouts after valid
samples. An explicit restored-link run passed 1,000/1,000 replies with zero errors in
69.999 s. I/O-only injection also passed: 400 expected failures, zero skipped releases
and zero observer expirations, with Zynq rejecting the supplied snapshots. Permits/HB remain inhibited; no operational health or recovery is claimed.

**2026-11-08 measured status and revised procedure:** See
[INA timing record](../../../../docs/ina_bench_timing.md). The bench INA responds
at 0x44. The revised bounded run completed **100/100 full acquisitions**, with
max release-to-end 7.697 ms, INA window 7.272 ms, ADC 0.315 ms and lateness
0.110 ms. The earlier 7 ms-budget run failed and is retained as historical evidence. The new requirement is 16 ms
period / 10 ms INA / 12 ms total work / 4 ms reserve. The diagnostic source now implements the revised envelope; both timing
profiles must be flashed from their rebuilt images. Follow the linked acceptance
procedure; a successful build does not establish physical timing or protection.

Standalone bench results are now recorded in the linked timing record. This is a
standalone ESP-IDF v6.1 diagnostic, not the operational monitor. It has no ARM,
UART recovery, healthy HB, or motor-permit assertion path. GPIO8/9/10/11 and GPIO21
are latched LOW before being enabled as outputs, following the existing bench
pattern. Reset/boot levels still depend on external board biases. Motor power
must remain disconnected; no Zynq is needed.

## Workload

Each absolute 16,000 µs release performs:

1. ESP32 ADC1 reads of **both battery taps**: GPIO1 (ADC1 channel 0, full pack),
   then GPIO2 (ADC1 channel 1, midpoint). Default one conversion each.
2. Optionally, four IPROPI ADC inputs GPIO4/5/6/7 (ADC1 channels 3/4/5/6).
3. Four logical INA slots, each reading Mask/Enable (0x06), shunt (0x01) and bus
   (0x02). **All twelve reads use the same physical INA226 handle/address.**

Before capture, the diagnostic probes **all addresses 0x40 through 0x4F** with
a 100 ms timeout per probe and prints each address/result. If any acknowledge,
it selects the configured preferred address (default 0x40) when present,
otherwise the lowest responding address, and reports `selected_address`.
All logical slots then use that address. An ACK does not verify device identity;
use the isolated INA fixture. If none acknowledge, the timing test is skipped
and the application idles with permits/HB LOW until reset. Discovery and its
console output are outside the measured cycles. Probes do not write registers.

I²C uses 100 kHz, SDA GPIO15, SCL GPIO16, explicit SCL wait
setting 1000 µs, external pull-ups, synchronous bus, one owner, no callbacks/retry.
This wiring is the diagnostic assignment: verify SDA/SCL against the actual module
and assembled board before connecting. Both ADC pins and I²C are owned exclusively
by this application. The INA must be powered and wired for compatible 3.3 V logic.
No motor-current source is required just to measure transport latency.

**Battery inputs must already be divided/buffered into the ESP32 ADC input range.**
Do not wire a battery tap directly to GPIO1/2. ADC attenuation is 12 dB, resolution
12 bits. Attenuation does not make a pin battery-voltage tolerant. A safe low-voltage
source may stand in for the divided taps during initial timing measurements.
No calibration, divider ratios, pack-voltage reconstruction, electrical protection
thresholds or battery-health decisions are implemented. CSV contains raw ADC codes;
it does not report actual pack volts. Repeated ADC sampling adds conversion work
but implements no filtering/averaging/settling policy.

Reading one INA four times approximates four devices' **transaction load**. It does
not simulate four independent currents, four conversion histories, harness loading,
shared ALERT behavior or device-to-device skew. Mask/Enable reads clear device
flags; later logical slots can see different flags. This program does not configure
the INA, change ALERT thresholds, write configuration, or require conversion-ready
for a successful timing sample. Record the device's existing configuration with the
run. `transport_complete` never means a fresh or healthy safety sample.

## Capture and deadlines

Default capture: 100 cycles (~1.6 seconds when on schedule). No USB formatting or
application logging inside the capture. Timestamp and error records use static
internal RAM; CSV is emitted **after** capture, then firmware remains idle. The
I²C driver's logs are disabled during capture to avoid distorting error paths.
This characterizes that log-silent configuration; enabled error logging is a
separate load case. Timer task/RTOS/interrupt activity still contributes normally.

An `esp_timer` one-shot wakes the owner at each absolute release; the timer does
not read sensors or toggle HB. ADC work must finish strictly before release+1000 µs.
INA work shares a deadline of ADC-end+10000 µs, therefore before release+11000 µs.
The CSV header reports the 16000 us period, 10000 us INA allocation, 12000 us
work budget and 4000 us reserve. The diagnostic does not implement policy/output
work and cannot validate that final 1 ms allocation.
There is no deliberate wait/padding to the end of the ADC allocation. Release-to-end
includes lateness, both battery ADC channels, optional IPROPI work and all INA calls.
A late start, ADC failure/overrun, I²C error, exhausted budget or late return aborts
that cycle. Missed releases are skipped, never caught up in a burst. Later diagnostic
cycles may retry solely for characterization; no operational fault latch is cleared.

The default mode calls the **same portable acquisition core** as the operational
adapter candidate, with a tracing bus that maps all four logical slots to one INA.
Timeout arguments are capped at 2 ms and floored to remaining representable ticks.
At 100 Hz the default mode rejects the zero-tick conversion without calling I²C.

An explicitly separate `DMOTOR_RAW_TIMEOUT_PROBE` mode passes the original 2 ms
argument directly to IDF, including at 100 Hz, to measure the diagnostic hazard.
It keeps the absolute before/after checks, but bypasses the core's zero-tick refusal.
It still aborts on first error/late return. A 100 Hz zero-tick attempt may fail before
all twelve reads; that is an observed result, not a reason to retry within a cycle.

**The stock driver cannot guarantee return by the deadline.** Internal waits,
NACK handling and automatic recovery can extend a call. Trace the resulting elapsed
time and overrun; the test never claims it cancelled an active transaction. A
nonreturning call can prevent CSV output. Do not disable watchdogs to force a pass;
retain reset logs and report incomplete capture. No in-cycle manual recovery is
performed. Controlled bus-stall experiments require a separately checked fixture;
do not short powered I/O pins indiscriminately.

## Build only

With the repository's pinned ESP-IDF v6.1 environment active:

```sh
cd software/esp32/diagnostics/ina_timing
idf.py -B build-1khz -D SDKCONFIG=sdkconfig.1khz set-target esp32s3
idf.py -B build-1khz -D SDKCONFIG=sdkconfig.1khz build
```

The separate 100 Hz / raw-timeout diagnostic profile prefers address **0x44**,
matching the responding bench module. It still scans all 0x4x addresses before
capture. This profile deliberately passes a 2 ms timeout that converts to zero
ticks at 100 Hz; it is a timeout-behavior experiment, not a reliable acquisition
profile.

Build this profile with:

```sh
idf.py -B build-probe100 -D SDKCONFIG=sdkconfig.probe100 \
  -D SDKCONFIG_DEFAULTS=sdkconfig.probe100.defaults set-target esp32s3
idf.py -B build-probe100 -D SDKCONFIG=sdkconfig.probe100 \
  -D SDKCONFIG_DEFAULTS=sdkconfig.probe100.defaults build
```

Inspect effective sdkconfig; defaults do not override an existing sdkconfig.
Project menuconfig options select preferred INA address, number of measured cycles,
raw conversions/channel (1..16), optional IPROPI channels, and raw-timeout mode.
For a 100 Hz **bounded** comparison, use a separate configuration with raw mode OFF.
For a 1 kHz **raw** comparison, use a separate configuration with raw mode ON.
Do not overwrite the UART/U1 or operational adapter configuration.

Capture RAM is compile-time limited to 160 KiB. Increasing ADC sampling/IPROPI may
require reducing cycle count; the build fails with a clear static assertion instead
of silently exceeding the diagnostic allocation. There is no PSRAM dependency.

These commands only build. Flashing is outside the current authorization. Once a
separately authorized deployment is complete, capture native USB output from reset
through `# DONE`. Console backpressure can delay post-capture output, not measured
sensor calls. Preserve the whole log, including reset/errors and profile metadata.
The finite diagnostic retains the existing default task/watchdog setup; it does
not implement or qualify the operational WDT/supervisor configuration.

## CSV and interpretation

| Record | Fields / interpretation |
|---|---|
| `# idf=...` | Actual IDF version, tick rate, physical address, I²C rate/stretch setting, ADC sampling, IPROPI and raw-mode selections |
| `cycle,...` | Scheduled release, actual start, ADC completion, finish, status, attempts/completions and skipped releases |
| `read,...,adc,...` | Logical ADC channel 0=full, 1=midpoint, 2..5=IPROPI0..3; start/end/duration, IDF error and raw code |
| `read,...,ina,...` | Logical device slot 0..3, register, start/end/duration, requested ms, converted ticks, IDF error and returned raw word |
| `# summary ...` | Completion count and observed maxima for release-to-end, complete ADC, attempted INA window and release lateness |

All times are monotonic microseconds. ADC-end=0 means the ADC stage did not
complete; INA-completed count excludes an error or late return even if its trace
contains received bytes. Returned data on failed reads are diagnostic only.
Timing instrumentation itself adds overhead; per-call timestamps bracket driver
execution, while cycle totals include tracing and loop work. Observe SDA/SCL on a
logic analyzer to separate wire/clock-stretch time from driver/scheduling overhead.

Report completion/failure counts alongside latency statistics; do not discard
failed or over-budget cycles when interpreting observed maxima. Compare two battery
reads, repeated ADC samples, then optional four-IPROPI load. A full four-motor
analog profile is six conversions minimum, not twelve I²C transactions alone.
Run across resets and relative tick phases and later under defined CPU/IRQ load.
The baseline deliberately starts no Wi-Fi/Bluetooth/UART/logging load. Observed
maxima are not a WCET proof, and this test does not qualify analog accuracy,
conversion freshness, output response or the proposed electrical thresholds.

## Host verification

The portable `timing_cycle.hpp` is exercised in `software/esp32/tests/` alongside
the acquisition tests. It injects battery ADC errors/delays, start/preemption
lateness, repeated-ADC/IPROPI load, every I²C failure/overrun position, and 100 Hz
bounded-versus-raw behavior. The fake bus checks the four-slot register sequence.
These tests validate sequencing/abort behavior; actual durations require ESP32-S3.

## Merged-main build evidence

Both the 1 kHz bounded profile and explicit 100 Hz raw-timeout probe pass fresh
ESP32-S3 builds using pinned ESP-IDF v6.1, with compiler caches disabled. The
portable acquisition/timing tests also pass native ASan/UBSan and ARM QEMU. See
[matched build record](../../../../docs/evidence/esp32_main_merge/inputs.json).
No target was flashed or measured; this does not establish timing qualification.
