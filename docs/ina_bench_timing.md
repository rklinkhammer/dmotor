# INA bench timing and revised monitoring budget — 2026-11-08

**Gate reconciliation (2026-11-08):** [G2 completed-case matrix](gates/G2.md#pre-pcb-acquisitionuart-results--2026-11-08)
records all supplied nominal, delayed/I/O-only fault, link-loss and restored-link
results. [Current next steps](system_status.md#next-implementation-and-validation)
separate work possible now from assembled-board validation. G1b/G1c/G2 remain
open with these subcases passed; G3–G6 and release G7 are not accepted.

**Pre-PCB implementation update (2026-11-08):** [Concurrent acquisition/UART diagnostic](prepcb_acquisition.md)
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

## Earlier standalone result — bounded 16 ms profile PASS

Robert supplied the [verbatim console excerpt](evidence/ina_bench_2026_11_08/bounded_16ms_excerpt.txt)
on 2026-11-08 after rebuilding/flashing the revised profile. The summary reports
**100/100 complete cycles**: two battery ADC reads and twelve INA register reads
per cycle. Cycle 99 contains all twelve successful INA reads with error 0 and
2 ms / 2 tick timeout arguments. This is operator-reported physical bench evidence,
not a new run performed by the documentation update.

| Observed maximum across 100 cycles | Value |
|---|---:|
| Release to acquisition completion | 7.697 ms |
| Complete ADC stage | 0.315 ms |
| INA acquisition window | 7.272 ms |
| Release lateness | 0.110 ms |

The INA window leaves **2.728 ms** in its 10 ms allocation. Release-to-completion
leaves **4.303 ms** before the 12 ms work deadline and **8.303 ms** before the next
16 ms release. Do not sum independent maxima or count lateness twice: the total
already includes lateness. The observed acquisition fits the revised target;
full monitor policy/output work and loaded timing are not measured here.

Scope: one physical INA (previously discovered at 0x44), four logical slots,
two ADC inputs, 100 kHz I2C, 1,000 Hz RTOS tick. Address and configuration are from
session context; the supplied excerpt omits startup headers, complete cycle CSV,
skipped-release counts and flashed-image hash. No hash is retroactively assigned
from a local build to the flashed device. Full-run summary is retained verbatim;
only the supplied final rows can be independently inspected. The 100 Hz
raw-timeout experiment has no supplied new result and is not marked passed.

The new success supports the larger-budget/one-tick-timeout explanation of the
previous failure, but does not isolate its cause experimentally. Fresh conversion
age, ADC calibration, sensor identity, all four physical devices, electrical
permit/HB behavior and protection/ARM are not qualified. The DONE line reports
commanded low outputs, not measured output voltages. G1c/G2/G3 remain open.

## Earlier 10 ms / 7 ms run — failed baseline

Robert supplied the following results from the standalone ESP32-S3 INA226/battery
ADC diagnostic, not the assembled four-device monitor. SDA=GPIO15, SCL=GPIO16,
100 kHz bus, 1,000 Hz FreeRTOS tick. Analyzer probes were at the module connector.
The initial capture decoded repeated address 0x40 write NACKs. Address scanning
found a responder at **0x44**; subsequent reads succeeded. The scan does not prove
part identity. Four logical slots read the same physical INA twelve times.

Cycle 99 excerpt (microseconds):

| Item | Observed value |
|---|---:|
| ADC channel 0 / 1 duration | 106 / 55 |
| Successful INA reads, ordinals 0–8 | 689, 585, 571, 570, 571, 572, 571, 570, 571 |
| Failed INA ordinal 9 | 292; timeout 1 ms / 1 tick; error 264; raw 0 invalid |
| Prior cycle 98 ordinal 9 | 295; timeout 1 ms / 1 tick; error 264 |

Supplied full-run summary:

```text
# summary cycles=100 transport_complete=0 max_release_to_end_us=6534 max_complete_adc_us=331 max_ina_attempt_window_us=6089 max_release_lateness_us=114
```

The first nine reads in cycle 99 used 2 ms / 2 ticks and returned error 0.
The failure coincided with the remaining-budget timeout shrinking to one tick.
Tick-boundary expiry is a hypothesis, not a proven diagnosis of this later error;
the original 0x40 address-NACK capture does not prove the cause at 0x44.
No complete twelve-read cycle passed in that earlier run. The maxima describe aborted cycles, not
successful full acquisitions. 689 us is the largest successful read in the
supplied excerpt, not a certified whole-run maximum or WCET. The full CSV and
firmware hash were not supplied with this excerpt. Screenshots were inspected
from operator files; they are not archived in this repository.

## Revised requirement

Replace the provisional 10 ms ESP32 monitoring cycle with **16 ms (62.5 Hz)**.
Interpret the requested 25% margin as **25% of the entire loop reserved**, rather
than adding 25% to measured work: work <= 12 ms, reserve >= 4 ms.

| Stage | Allocation | Deadline from scheduled release |
|---|---:|---:|
| GPIO checks and all required ADC conversions | 1 ms | < 1 ms |
| Up to twelve INA register reads | 10 ms | < 11 ms |
| Freshness/protection policy and output/HB decision | 1 ms | < 12 ms |
| Unallocated loop margin | 4 ms | Next release at 16 ms |

The planning estimate is 12 × 689 us = **8.268 ms** for INA reads; round the INA
allocation to 10 ms for overhead/variation, then add 1 ms ADC and 1 ms policy.
12 / 0.75 = 16 ms. This is a measurement-informed allocation, not qualification.
The reserve must remain unused in the accepted worst-case workload; do not treat
16 ms as permission for normal work to exceed 12 ms. Task lateness/preemption
consume the scheduled-release budget. No catch-up cycles or healthy output from
partial, failed or late work. The Zynq 1 ms motor loop is unchanged.

## Implementation and test disposition

**Diagnostic implementation updated on 2026-11-08:** Shared budget constants now
set the 16 ms period, 10 ms INA allocation and 11 ms acquisition ceiling. Both
bounded and raw-timeout paths use the new envelope. Portable tests cover measured
read-duration replay, exact deadline rejection, early abort and overflow. The
operational deployment remains outstanding; the recovery simulator now uses 16 ms.
See [the candidate supervisor and physical-health implementation](monitor_next_steps.md) for remaining
work; there is no implemented policy/output phase in this standalone test.

1. Flash the newly rebuilt bounded 1 kHz or 100 Hz raw-timeout profile from its
   separate build directory. Confirm the CSV header reports `monitor_period_us=16000`,
   `ina_budget_us=10000`, `work_budget_us=12000`, `reserved_margin_us=4000`.
   Scan/report 0x44 for this fixture; production population/address assignment
   remains explicit, not auto-discovered. Older CSV/binaries retain historical
   meaning and must not be relabeled as results under the new budget.
2. Revised bounded diagnostic: absolute 16 ms releases, ADC before release+1 ms,
   INA before min(ADC-end+10 ms, release+11 ms), and 1,000 Hz FreeRTOS ticks.
   Keep deadline-aware waits (2 ms cap), before/after checks and early abort;
   report any one-tick truncation rather than claiming it guarantees 1 ms runtime.
   Do not extend a call beyond remaining budget just to obtain success.
3. Capture at least 100 cycles (nominal 1.6 s after startup scan), retain all CSV,
   probe results, skipped releases, error counts, read counts, duration and lateness
   maxima. Require all 12 reads and required ADC inputs on every accepted cycle;
   100/100 completion is a baseline criterion, not a WCET proof. Analyzer: 10 MS/s,
   SDA/SCL assigned correctly, 1.65 V threshold and pre-trigger capture from reset.
4. The 100 Hz raw-timeout probe deliberately converts 2 ms to zero ticks. Use the
   same revised cycle envelope, but keep it a separate negative
   experiment; its failures are not evidence that the bounded profile is broken.
   Address discovery's 100 ms probe timeout is outside measured acquisition.
5. Add all populated sensors, six ADC inputs, intended filtering, policy/output
   work and representative scheduling load. Measure release-to-final-output work
   <12 ms with at least 4 ms reserve per 16 ms period; a two-ADC/no-policy fixture
   cannot establish this. Include error/stall paths and independent inhibit
   supervision; stock IDF call timeouts do not provide a hard execution bound.
6. Recheck freshness, ARM qualification, heartbeat and detection limits below.
   Keep permits/HB low in acquisition diagnostics; no motor/ARM qualification.

## Dependent timing requirements

- Retain the provisional 25 ms conversion-age limit; do not relax it to hide stale
  data. Validate conservative conversion age with the longer read schedule and
  actual INA conversion/averaging settings. Successful I2C does not prove freshness.
- Ten healthy ARM checks now use 16 ms cadence (160 ms nominal qualification
  allocation); 20 ms press/release debounce remains elapsed-time based. Update
  recovery model/tests before claiming they validate this revised schedule.
- Under the existing three-stale-observation plus one-output-cycle model, the
  proposed bound becomes 25 + 4 × 16 = **89 ms**, replacing 65 ms. This is conditional
  on the age and scheduling assumptions; an acquisition failure still inhibits
  in the detecting cycle, without waiting for three failures.
- The 500 ms LVC persistence plus two scheduling periods becomes **532 ms**,
  replacing 520 ms, excluding front-end/conversion delay; thresholds remain open.
- Operational HB may advance only after valid complete work. If toggled once per
  cycle, nominal edge spacing is 16 ms and full-wave frequency is 31.25 Hz.
  Verify receiver edge/pulse semantics and timeout compatibility; do not change
  watchdogs blindly. The separate U1 synthetic 50 Hz heartbeat remains unchanged.
- Independent fault ISR response targets and PL protection remain unchanged.
  U03/U04/U06, four-device loading, live recovery and assembled-board acceptance
  remain open. Commissioning-first release scope is unchanged.

## Next implementation and validation steps (ordered)

The completed pre-PCB diagnostic sequence is consolidated in [G2](gates/G2.md).
Use [current system status](system_status.md#next-implementation-and-validation)
for the prioritized plan and gate impact:

1. Improve receiver rejection reasons and retain complete run provenance.
2. Implement physical-device conversion readiness/age and ADC calibration handling;
   test stale-but-readable, invalid configuration and missing/partial inputs.
3. Migrate the recovery model to 16 ms and implement operational safety-task,
   independent inhibition and fault-epoch/output serialization; test late work/races.
4. With assembled boards, qualify four sensors, all ADC channels and complete
   loaded work <12 ms with >=4 ms reserve, including real fault/stall behavior.
5. Complete U1 electrical shutdown, heartbeat/watchdog and joint recovery gates
   before powered commissioning. No automatic/live motor recovery is implied.

The raw 100 Hz timeout probe remains optional zero-tick characterization, not a
prerequisite for the successful bounded 1 kHz profile. Neither its negative
results nor synthetic I/O injection establish real-device freshness/protection.
