# G1c monitor acquisition and protection design

**Validation update (2026-11-08):** [G2 operator evidence](gates/G2.md#pre-pcb-acquisitionuart-results--2026-11-08)
now covers 10,000 successful concurrent acquisitions, 1,000 verified UART replies,
400/400 delayed-injection expirations, 400 I/O-only failures without overruns,
link-loss timeouts and explicit restored-link PASS. These are diagnostic subcase
passes, not acceptance of G1c/G3 or SYS-03–06. Sensor freshness/calibration, loaded
policy/output work, isolation and electrical shutdown remain open. See the
[prioritized implementation/validation plan](system_status.md#next-implementation-and-validation).

Prepared 2026-11-06 for Robert's review. **Design candidate, not operational
firmware or gate acceptance.** Firmware belongs in `dmotor/software/esp32/`.
This record replaces no hardware source and selects no electrical trip threshold.
It makes the remaining decisions needed by SYS-03–06 / U03/U04/U06 explicit.

Sources: rev-A [monitor design](../../motor-interface-integrated/monitor/README.md),
[control architecture](../DMOTOR_CONTROL.md), [recovery v4](recovery_transition_contract.md),
and the pinned ESP-IDF v6.1 headers/driver implementation. Board population and
installed resistor/shunt values must be checked against the actual assembly.

## Acquisition inventory

Population is an inhibited-only configuration mask, never auto-discovered from
which sensors happen to answer. Both battery taps are always mandatory for the
operational profile. The existing UART diagnostic is a separate no-ARM profile.

| Input | Required / interface | Work per cycle | Timestamp and failure rule |
|---|---|---|---|
| Battery full, midpoint | Always; GPIO1/2 ADC1 | Two conversions minimum; any filtering multiplies this count | Timestamp conversion start/end; validate each value and their relationship; missing, invalid or expired data prevents a healthy cycle |
| IPROPI0–3 | Each populated motor; GPIO4–7 ADC1 | One conversion/channel minimum | Same time accounting; calibrated current proxy, not INA input current; disconnected sensor detection needs bench characterization |
| INA2260–3 | Each populated motor; I²C GPIO15/16, addresses 0x40–43 | Three 16-bit register reads/device: Mask/Enable, shunt, bus | Require fresh-conversion evidence; failed/partial group never refreshes the sample timestamp |
| nFAULT0–3 | Each populated motor; GPIO12/13/14/42 | Edge ISR plus level check each cycle | Asserted level inhibits immediately; no debounce before inhibit |
| ALERT | Shared GPIO47 | ISR plus level check; preserve cause before register read clears device flags | First inhibit all; resolve contributing INA in task context |
| STOP / HALT | GPIO40 / GPIO39 | ISR plus each-cycle level check | STOP not solid low or HALT asserted inhibits; release cannot automatically ARM |
| ARM | GPIO41 | Sample every cycle | Use v4 press/release, qualification and epoch rules; no held-button rearm |

INA shunt/bus results are not claimed to be simultaneous with ADC samples or each
other. Use shunt voltage with measured shunt resistance for feed current; optional
current/power register telemetry must not add unbudgeted safety-path transactions.

The INA conversion-ready flag is cleared by reading Mask/Enable. Save the returned
fault flags before clearing them; disable conversion-ready signalling on the shared
ALERT line so healthy conversions do not masquerade as faults. A ready flag is not
a hardware timestamp. Track the previous successful flag-clear time as a
conservative lower bound for the new conversion completion; do not stamp old
results as freshly converted merely because a read just completed. After boot,
configuration change or missing observation history, establish a clear/ready cycle
before accepting freshness. A stalled converter must fail the freshness test even
if I²C keeps returning the same valid bytes. These register semantics and the
588 µs × two conversions × four averages configuration come from the
[TI INA226 datasheet](https://www.ti.com/lit/ds/symlink/ina226.pdf).

## Absolute cycle budget

Required target cycle is now 16,000 µs (see [measured basis and migration procedure](ina_bench_timing.md)), measured from an absolute scheduled release,
not from when a late task eventually runs. No catch-up HB pulses. At each operation,
check elapsed time and reserve time for output clear/state evaluation. Abort the
cycle on exhausted budget; retain permits low and do not toggle HB. Never execute
all four device timeouts after the cycle deadline has expired.

At 100 kHz a pointer-write + repeated-start 16-bit read consumes at least 45 SCL
clocks: address/write, pointer, address/read, two data bytes with ACK/NACK slots.
Thus 12 reads consume **5,400 µs minimum wire time**, excluding START/STOP timing,
stretching, software, arbitration and preemption. This is not a worst-case bound.
Three reads × four devices × the historical 2 ms timeout would instead allow
**24 ms**, already exceeding the cycle. A per-transaction timeout alone is invalid.

Revised allocation to validate, not an accepted WCET. Scheduled-release deadlines
are 1 ms ADC, 11 ms INA, 12 ms final commit; the remaining 4 ms is reserved:

| Stage | Budget | Failure at boundary |
|---|---:|---|
| GPIO checks and six ADC conversions | 1,000 µs | Inhibit, reject incomplete sample set |
| All populated INA reads combined | 10,000 µs | Abort at shared deadline, no subsequent reads this cycle |
| Policy, freshness, permit commit, HB decision | 1,000 µs | No enable/HB on incomplete commit |
| Reserved loop margin (25% of 16 ms) | 4,000 µs | Work exceeding 12 ms fails the budget; no phase drift |

This envelope is feasible only if measured ADC/I²C execution fits it. Keep 100 kHz
as the design baseline; raising it requires checking the harness capacitance and
rise time. Do not silently change to 400 kHz to make the arithmetic pass.

INA refresh is nominally 4.704 ms; using the datasheet's 646 µs maximum conversion
time gives 5.168 ms for the same averaging setting. Neither figure includes the
read schedule or analog response. The retained 25 ms age limit and revised 89 ms stale-fault / 532 ms LVC
bounds remain conditional. Battery RC settling, ADC filtering and task lateness
must be added to end-to-end limits. HB is suppressed on the first unhealthy cycle,
even if a fault persistence policy has not yet latched its final cause.

## Protection policy to review

Cause names below are proposed symbolic categories. Numeric wire bit allocation
is **pending** in the shared monitor schema; no independent ESP32 enum is to become
a second authority. First cause plus accumulated causes survive until explicit
inhibited recovery; logging loss cannot lose the inhibit latch.

| Cause / input | Units / valid range | Trip / persistence / hysteresis | Action and clear |
|---|---|---|---|
| STOP, HALT, NFAULT, ALERT | GPIO levels, as above | Immediate assertion; release qualification per v4 | Clear all permits in ISR, target ≤100 µs; exchange and physical ARM required |
| ACQUISITION / STALE | Monotonic µs; no future timestamp or failed read accepted | D10: age <25 ms at cycle commit; no continuous age-based permit expiry. Invalid cycle or next release+12 ms liveness expiry inhibits | No healthy HB for failed cycle; bounded fault latch; healthy data alone never rearms |
| CALIBRATION / CONFIG | Versioned validated record and population | Missing/corrupt/unsupported is invalid; no startup bypass | Inhibit until explicit valid installation and recovery |
| BATTERY_RANGE | Full and midpoint mV; 0 < mid < full, ADC not clipped | Calibrated limits and tolerance pending | Inhibit; diagnose wiring separately from low battery |
| BATTERY_LVC | Lower 2S = mid; upper 2S = full − mid | Trip mV, release mV pending; 500 ms persistence candidate | Proposed 532 ms after first qualifying sample, plus analog/acquisition delay; explicit recovery |
| BATTERY_IMBALANCE | Absolute difference of the two 2S voltages, mV | Threshold, persistence and release hysteresis independently pending | Inhibit; cannot reuse LVC persistence implicitly |
| MOTOR_CURRENT | IPROPI mA, INA feed mA as separate quantities | D10 best-effort current/time policy implemented; actual calibrated thresholds and hardware ALERT settings pending | Inhibit; feed current is not winding-current proof |
| INTERNAL / DEADLINE | State, timer, storage and arithmetic validity | Immediate; overflow/exhaustion fails closed | Inhibit; no automatic retry restoring permits |

No per-cell voltage or temperature sensor is fitted in this inventory. Two 2S
measurements cannot demonstrate individual-cell balance. Such a requirement needs
additional sensing or an explicitly limited scope, not invented telemetry.

## Calibration and lifetime storage

Proposed calibration record schema 1: fixed little-endian header (magic, schema,
length, generation), board identity, population mask, both mandatory battery
calibrations, four channel records (ADC offset/gain, shunt value and calibration),
policy values with explicit units, and CRC32 over all preceding bytes. Define exact
field widths/ranges and golden vectors in one shared schema before implementation.
Use signed offset and checked fixed-point gain arithmetic, reject overflow, NaN-like
sentinels, duplicate IDs, invalid masks and zero/negative gains.

Two NVS blob slots: while inhibited, validate candidate in RAM; write inactive
slot, commit, read back and revalidate before selecting its higher generation.
Equal-generation different content, an unknown schema, or a damaged newer record
requires service; never silently restore older thresholds. Interrupted update may
retain the previous valid record only with explicit update-failure indication and
continued inhibition. No defaults that authorize ARM. Use a deliberate factory
installation operation; NVS error must not trigger erase-and-reinitialize.

Endpoint lifetime is a separate record from calibration. PS journal policy already
exists. ESP32 must durably reserve the next nonzero 64-bit lifetime before HELLO;
commit/readback failure, exhaustion or lost journal keeps it inhibited. Calibration
rollback cannot roll back lifetime. Flash erase, board replacement or restored
backup needs an explicit new identity/provisioning procedure; never start at 1
against an existing peer after journal loss. Actual power-cut qualification remains
required on each storage backend.

Error budget per channel records instrument accuracy, ADC residual/attenuation,
divider/shunt tolerance and temperature drift, installation gain/offset error,
noise/filter delay and allowed headroom to thresholds. For upper-pack subtraction,
combine both tap errors conservatively. Calibration at one point does not establish
linearity across the range. Robert supplies measured values, population and chosen
battery/motor limits before operational configuration can be implemented.

## Pinned IDF isolation audit

Inspected ESP-IDF v6.1 image digest
`81893c71bb5e570088901f21def8684c25cd2a9020281bd01b843a7655edb18c`.
The **original U0 UART diagnostic** sdkconfig audited here had 100 Hz ticks, 5 s task WDT with panic disabled,
300 ms interrupt WDT, GPIO control IRAM disabled and no PSRAM. These defaults are
not an operational safety configuration. The subsequent [U1 diagnostic](u1_gpio_bench.md)
uses 1000 Hz ticks for synthetic HB; this does not qualify operational isolation.

| Resource | Operational design requirement | Verification still required |
|---|---|---|
| ISR / permit clear | Code and every callee in IRAM, latch/constants in internal DRAM; no log, allocation, mutex, UART, I²C, ADC or NVS calls; direct GPIO clear | ELF/map transitive audit plus scope under cache-disabled conditions |
| Permit assertion | Single safety-task writer; serialize final fault-latch check and output set against ISR so a late task cannot undo ISR clear | Inject fault at each check/write boundary; include cross-core accesses |
| I²C deadlines | One owner; driver timeout limited by remaining cycle; no unbounded bus recovery | v6.1 driver converts milliseconds with `pdMS_TO_TICKS` (`i2c_master.c:679`); at 100 Hz, 2 ms truncates to zero ticks. Choose finer tick/config and measure blocking/error exits |
| Watchdog | Subscribe actual safety task; feed only after completed healthy cycle | `esp_task_wdt_config_t.timeout_ms` supports candidate 500 ms via init/reconfigure; enable panic/reset and test, do not rely on whole-second default |
| Flash/cache | Updates/calibration/NVS only inhibited, admission revoked first, HB qualification re-established afterward | Verify both cores, cache disable, failed writes, interrupted updates and reset |
| Diagnostics | Bounded RAM queue, lossy logs, native USB; UART1 protocol only | USB unplug/backpressure and UART flood cannot delay safety cycle or clear sticky faults |
| Startup | Write permit/HB latches low before output enable; recovery starts unarmed | Measure power/reset/brownout through GPIO initialization with real board biases |

## Stop point and next implementation

G1c is ready for design review, **not accepted**. Record in the existing gate notes:
actual populated channels/resistor/shunt values; selected voltage/current/imbalance
limits and hysteresis; agreement on the budget and calibration/lifetime recovery
policy. Candidate physical-health/calibration and safety-core/IDF adapters now
exist with host fault tests. One-INA health and wrong-address rejection passed on
the fixture; integrate the full operational application only after these remaining
policy choices and timing/age proofs are established. Do not infer these
choices from the successful USB or UART diagnostic.

G1a's [remaining proof checklist](g1a_client_status.md#remaining-g1a-gate) must also
be reviewed before connecting live monitor recovery to daemon motion admission.

## Deadline implementation increment (2026-11-07)

The [I²C deadline adapter candidate](g1c_i2c_deadline_adapter.md) supplies portable
absolute-deadline acquisition and deterministic fault/preemption tests, plus a
1000 Hz guarded IDF adapter. Stock-driver timeout reuse/recovery means that an
argument is not a hard whole-call bound. Independent deadline inhibit and target
qualification remain required; no operational configuration or gate is accepted.

## Implementation follow-up — 2026-11-08

See [receiver rejection reporting, physical sensor health and 16 ms safety candidate](monitor_next_steps.md).
Host tests and inhibited firmware builds advance implementation evidence only.
Existing hardware results remain tied to their original diagnostic images;
operational protection, electrical acceptance and live recovery are not claimed.

Current physical evidence and pending cases are consolidated in
[G2](gates/G2.md#physical-ina-health-results--2026-11-08). G1c remains OPEN:
no independently validated conversion bound, four-device operational budget,
known-reference calibration or electrical shutdown acceptance is recorded.

**Long one-INA physical-health result:** [10,000-cycle nominal capture](evidence/sensor_health_long/physical-run.txt)
passes with 9,999 good and one warming, max 3,905 us, lateness 24 us, calculated
age bound 20,240 us, and zero bus/guard/late/deadline counts. This advances U03/U06
and G2 diagnostic evidence only. The 4,760 us remaining age validity does not
cover another 16 ms cycle; conversion-bound proof, sustained output qualification,
four-device timing, calibration and electrical gates remain open. See
[next work](monitor_next_steps.md#validated-10000-cycle-result).
