# G1c I²C deadline adapter candidate — 2026-11-07

**2026-11-08 requirement update:** [INA bench evidence and revised budget](ina_bench_timing.md)
sets a 16 ms monitor cycle, 10 ms INA allocation, final work <12 ms and 4 ms
reserved margin. The portable adapter and standalone diagnostic now use this
budget. Operational supervision/output integration and physical qualification
remain outstanding.

Implementation increment for U03/U04, on branch `esp32`. Not G1c/G3 acceptance,
not a complete operational monitor. No electrical thresholds selected. No hardware
flash or motor authorization. UART/U1 diagnostics retain their separate configuration.

## Tick choice

| Property | 100 Hz | 1000 Hz |
|---|---:|---:|
| Tick quantum | 10 ms | 1 ms |
| `pdMS_TO_TICKS(2)` in audited IDF | 0 | 2 |
| Smallest positive integer-ms input yielding a tick | 10 ms | 1 ms |
| Tick periods per 16 ms cycle | 1.6 (not integral) | 16 |
| 1–9 ms requests | zero ticks | 1–9 ticks |

Zero ticks means no blocked wait at the RTOS primitive, not a 0 µs driver-call
bound. Commands may already have started, error cleanup may execute, and the
call can fail immediately or return later. Never pass zero as a short timeout,
never pass -1 (infinite), and never silently clamp to a minimum tick beyond budget.
The portable converter floors remaining µs to complete ticks after a 2 ms cap;
it refuses an unrepresentable wait. At 100 Hz this profile performs no reads and
fails closed. The IDF component explicitly requires 1000 Hz. Supporting a slower
monitor at 100 Hz would require a separately reviewed budget/configuration.

RTOS waits count tick boundaries: a one-tick wait may expire almost immediately
if issued just before a boundary. Finer ticks improve resolution, not guaranteed
minimum transaction execution time. A failed early wait rejects the cycle; do not
retry it twelve times. One millisecond is longer than the 450 µs wire minimum,
but overhead/stretching may still prevent success. 1000 Hz adds approximately
900 tick interrupts/second relative to 100 Hz on each ticking core, and may add
scheduling work, jitter and power. Measure this; it is not automatically ten times
the CPU cost. `esp_timer_get_time()` provides monotonic µs accounting independently
of the tick configuration. Tick scheduling alone is not the deadline clock.

## Absolute acquisition contract

The caller supplies the **scheduled release**, never the actual task-start time.
Keep phase on absolute 16,000 µs releases. On lateness, reject the affected release;
skip missed releases to the next future one, with no catch-up HB. ADC/GPIO work
precedes `run()` and must finish by release+1000 µs. All INA reads share
release+11000 µs as the exclusive deadline. This retains the proposed 1000 µs
policy/output allocation plus 4000 µs reserved loop margin. The diagnostic uses
ADC-end+10000 µs, which is earlier than release+11000 µs. Allocations require measurement.

`Acquisition::run` has one owner, fixed storage, at most twelve synchronous calls,
no allocation/log/retry/recovery, and a validated inhibited-only population mask.
It reads Mask/Enable, shunt and bus at 0x40..0x43 in that order for each installed
motor. Before **every** call it computes the remaining time and selects the smaller
of that and 2 ms, floored to nonzero representable ticks. After every return and
before publication it checks the same absolute deadline. Equality is failure.
Task preemption consumes the same budget, including before publication. Backward
clock movement, invalid masks, overflow, early invocation and late start fail closed.

No subsequent read occurs after a detected overrun, error or unrepresentable wait.
A partial group/cycle has no usable raw publication: raw data are cleared, status
is failed, and read count remains diagnostic only. Failure latches in the owner;
a later successful cycle cannot clear it. Reconstructing the owner is permitted
only as part of inhibited startup/recovery, never as an automatic retry/rearm.
Raw Mask/Enable flags on a *successful* cycle are retained for policy evaluation.
On a failed cycle the cause is acquisition/internal fault; flag clearing may have
lost detailed INA causes, so there is no rearm from subsequently healthy bytes.

The adapter does not equate I²C success with a fresh INA conversion. Integration
must track the previous successful Mask/Enable clear, require new ready evidence,
validate startup/configuration history, and retain conservative conversion age as
specified in [G1c](g1c_monitor_design.md). Partial cycles cannot refresh that
history or the last accepted sample timestamp. If history is ambiguous after a
failed flag read/clear, re-establish it while inhibited. Conversion-ready signalling
must stay off the shared fault ALERT signal. ADC completeness/age are separate
mandatory proofs, including both battery taps.

## The important bound limitation

The [v6.1 driver source](https://github.com/espressif/esp-idf/blob/v6.1/components/esp_driver_i2c/i2c_master.c)
contains `s_i2c_synchronous_transaction`, `s_i2c_transaction_start` and
`s_i2c_send_commands`. The same converted timeout can be used for bus-lock take,
command-semaphore take and event-queue wait, followed by a NACK bus-busy loop
using a strict `>` tick check. Start may reset/clear a busy or previously timed-out
bus. Error exits reset the FSM. Recovery and log output have additional cost.
Consequently **even a 1 ms argument is not a 1 ms whole-call bound**. Configured
SCL stretch timeout is another hardware setting, not the acquisition deadline.
The adapter intentionally does not promise that flooring a timeout cancels an
in-progress call at release+11000 µs.

The implemented bound is on transaction count and **acceptance of results**.
A late return is always rejected and no later device is called. A strict execution
WCET cannot be established by this wrapper around the stock driver. The 5400 µs
minimum wire time leaves 4600 µs in the proposed INA allocation for all other
costs; it is a feasibility lower bound, never evidence of a worst-case upper bound.
Summing twelve 2 ms caps (24 ms) is not the cycle policy or a driver WCET either.

Operational integration must use an independent deadline supervisor to revoke
acquisition qualification at the absolute deadline even if the owner is blocked.
It must use the same ISR-safe inhibit latch/output-clear path as physical faults,
not acquire an I²C lock or wait for the worker. At release, revoke the previous
cycle token; publish a new token only after complete ADC/INA/freshness/policy
proofs. A generation/fault epoch prevents a late worker result from restoring it.
At the final GPIO assertion/HB decision, serialize epoch and deadline checks with
the fault ISR using the reviewed cross-core critical section. A deadline callback
must also be invalidated by generation so an old timer cannot clear a newer cycle.
This supervisor/output path is a required integration contract, **not implemented
or qualified by these components**. The components contain no output assertion.

Do not hold permits healthy while a blocking driver runs on the assumption its
argument is a hard bound. If independently scheduled supervision itself stalls,
no autonomous timer ISR may continue toggling healthy HB. Independent PL HB
expiry and task/interrupt watchdogs provide separate fallback bounds; they do not
prove the 11 ms acquisition bound. The operational safety task must be subscribed
to WDT and feed only after a healthy committed cycle. Hardware/cache/interrupt
failure response remains U01/U04 work.

For a strict wall-clock I²C execution bound, acceptance requires either an audited
IDF driver change threading one absolute deadline through all waits/loops/reset
paths, or a separately qualified peripheral worker plus independent inhibit.
Do not delete a blocked task or free/reset its bus/buffers concurrently: the driver
may still own them. No async callbacks/queues are allowed by this adapter. After a
stall/error, keep inhibited, stop scheduling operational reads, and recover/recreate
bus only after its owner has returned/quiesced. Any recovery has its own inhibited
budget and watchdog/reset escalation. A nonreturning owner requires supervised
reset; there is no in-cycle bus-clear retry. Set and measure `scl_wait_us` explicitly
in board initialization rather than treating the driver default as a safety limit.

## Qualification boundary

`acquisition_qualified()` is a necessary-condition helper for a complete, current
result and an unlatched fault. Its `other_safety_proofs` argument must represent
validated ADC, INA ready/history/age, calibration, electrical policy, physical inputs,
recovery/ARM and current epoch. It is never permission by itself. Re-evaluate at
serialized output commit; do not cache true. Incomplete/late acquisition always
returns false regardless of other proofs. No HB generator or motor permit writer
exists in this increment. Electrical protection, storage, GPIO races and protocol
integration remain unimplemented and must not be replaced with constant true.

## Build and deterministic verification

Host, independent of ESP-IDF and the Linux project:

```sh
cmake -S software/esp32/tests -B /tmp/dmotor-acquisition-build
cmake --build /tmp/dmotor-acquisition-build
ctest --test-dir /tmp/dmotor-acquisition-build --output-on-failure
```

Or directly:

```sh
g++ -std=c++14 -Wall -Wextra -Werror -Wconversion \
  -fsanitize=undefined,address -fno-omit-frame-pointer \
  -Isoftware/esp32/components/acquisition/include \
  software/esp32/tests/acquisition_test.cpp -o /tmp/dmotor-acquisition-test
/tmp/dmotor-acquisition-test
```

Tests inject 450 µs successes, failure at every read, delayed success beyond the
deadline at every read, exact deadline equality, remaining sub-tick budget,
100 Hz rejection, preemption before/between reads and at publication, clock reversal,
all installed masks, invalid configuration/overflow, expired qualification and sticky
failure across a later successful cycle. They prove portable state/acceptance rules,
not hardware return times. A fake bus may return arbitrarily late specifically to
ensure the test does not assume the requested timeout is respected.

With the pinned ESP-IDF v6.1 environment active, compile/link only:

```sh
idf.py -C software/esp32/build_check set-target esp32s3
idf.py -C software/esp32/build_check build
```

The link-check application has no bus initialization, safety task or GPIO writes;
it is not deployable monitor firmware. Defaults request 1000 Hz; an existing
sdkconfig overrides defaults, and the adapter compile-time guard rejects 100 Hz.
Do not flash it. Integrate components into the future operational project via
`EXTRA_COMPONENT_DIRS`, `REQUIRES monitor_i2c`, dedicated bus/handles and internal
RAM task stack/buffers. Use GPIO15/16 per reviewed board mapping, 100 kHz, 7-bit
addresses 0x40..0x43, queue depth zero, no callbacks and no concurrent bus users.
Configure bus/devices only while inhibited. Do not increase to 400 kHz implicitly.

Target build is required before claiming ESP-IDF API/toolchain validation. Native
tests alone do not compile the IDF implementation. Record effective sdkconfig,
IDF version/image digest, compiler, map/ELF and source commit with build evidence.

## Required inhibited ESP32-S3 measurements

Measure release lateness, each read entry/exit and complete ADC/INA/policy path
with µs traces and scope GPIOs, across tick phases and CPU/IRQ load. Exercise NACK,
missing device, held SDA/SCL, stretching, late/error exits, bus-busy recovery,
preemption at each boundary, interrupt/cache-disabled intervals and USB/UART flood.
Measure supervisor-to-permits-low latency independently of blocked driver return,
and confirm no late success/HB/permit restoration after a fault epoch. Record WDT
and PL expiry bounds separately. Check internal RAM/IRAM placement and cross-core
fault/output races. Test delayed conversion-ready and identical data, not only
transport faults. Measure ADC/filter/analog settling and conversion/read skew.

Keep U03/U04/U06 and 16 ms feasibility open until measured maxima plus justified
margin fit the reviewed allocations. If they do not, revise the acquisition design,
period or independently supervised execution architecture explicitly. No thresholds
or gate acceptance follow from host tests or an IDF build.

## Isolated measurement preparation

The [INA + battery ADC diagnostic](../software/esp32/diagnostics/ina_timing/README.md)
implements a finite RAM capture of both battery ADC channels followed by the same
INA read four times (twelve transactions), with per-call times and errors. Optional
IPROPI acquisition and ADC sample counts extend the analog workload. A separately
labelled 100 Hz raw-timeout probe deliberately measures zero-tick driver behavior;
it does not weaken the operational adapter guard. The merged-main [clean build](evidence/esp32_main_merge/inputs.json) passes both
target timing profiles and the adapter link harness. No physical measurement,
flashing or safety acceptance is recorded.
