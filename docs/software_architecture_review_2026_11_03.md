# Zynq and ESP32 software architecture review

Date: 2026-11-03. Boards are ordered; this review treats the current rev A
hardware as the integration target. Recommendations below are proposed changes,
not newly approved requirements or evidence of physical qualification.

## Assessment

Retain the partition: Zynq PL owns deterministic motor control and independent
motion timeouts; Zynq Linux owns intent, ownership and motion planning; ESP32-S3
owns sensor acquisition, electrical protection policy and physical permits.
The existing Zynq velocity runtime is a useful foundation. The combined system
is **not yet implementation-ready at its protection/recovery boundaries**.
Resolve R1–R5 before enabling the ordered boards as an integrated motor system.
ESP32 firmware remains a plan, not an executable implementation.

Reviewed working trees: dmotor HEAD `7d92ee9`, motor-interface-integrated HEAD
`5ebcebe`, including the existing uncommitted DMOTOR_CONTROL.md changes. Sources
include DMOTOR_ARCH.md, DMOTOR_SW.md, DMOTOR_CONTROL.md, component/status/build
documents, software protocol/runtime/daemon/kernel access, board BD/XDC, protection
RTL, and the monitor README/FIRMWARE.md/issue register and circuit intent.
This is an architecture and selected implementation review, not an exhaustive
line-by-line audit, new routed FPGA qualification, or electrical certification.

## Existing foundations to preserve

| Layer | Present | Still needed for this system |
|---|---|---|
| PL | Shared four-context velocity engine, atomic target batches, independent watchdog/target age, encoder/PWM, latches | New board I/O, monitor qualifier, revised coast/recovery contract, new integration evidence |
| Linux | Typed protocol, exclusive kernel access, sole-owner daemon, maintenance lease, bounded direct-duty commissioning | Monitor protocol/recovery, bounded diagnostics, scheduling evidence, normal vehicle ownership policy |
| ESP32 | Pin map and host-testable C/FreeRTOS architecture plan | Implementation, complete timing/state/protocol/configuration contracts, tests |
| Vehicle | Processor ownership of trajectory, position and odometry is specified | Planner, coordinate frames, localization freshness, stopping/completion acceptance |

Preserve atomic batch installation, separate target freshness and authorization,
explicit rearm, fault-first output gating, and separation of command acceptance
from application and physical completion. No reason was found to move trajectories
back into PL or motor PI into the ESP32.

## Findings, ordered by integration priority

### R1 — High: software HALT does not reach the monitor coast output

Evidence: `software/linux/driver/dmotor_access.c:halt_all()` asserts AXI GPIO;
`platforms/arty-z7-20/vivado/design_1.bd` connects that GPIO to `halt_async`,
but connects the monitor-facing `mc0_halt`/`mc1_halt` outputs to `shared_halt`.
`rtl/safety.sv:37` generates halt_out only from local faults;
`rtl/motor_io.sv:108–109` inhibits on external HALT but deliberately excludes
external HALT/latches from shared_halt. This preserves the old no-echo rule.

Consequently a healthy controller receiving Linux HALT can stop PWM without
asserting the monitor's hardware coast gate or its firmware HALT input. With
monitor permits still high, zero EN is braking in PH/EN mode, not coast.
The distinction is explicit in the [carrier truth table](https://www.pololu.com/product/5238).
Driver confirmation of disabled/blocked status therefore does not confirm coast.

Required design: distinguish local-fault reporting from the outward coast request.
Define which PS HALT, HB-loss, local-fault, reset and disabled conditions assert
the monitor line; preserve cause attribution without blindly echoing a feedback
loop. Explicitly decide the physical semantics of READY, STOP and DISABLE too:
the new motor connector does not use the old per-channel drive_enable output.
Test software HALT from active motion and READY, client exit, service death,
and explicit disable; observe PWM, HALT, permits and actual DRVOFF together.

### R2 — High: ordered-board PL integration is absent

Evidence: active XDC/BD exposes only motor 0/1 PWM/DIR/QA/QB, old enable/fault
signals and shared HALT outputs. There is no HB input, HB qualifier or monitor
UART interface. The hardware has four internal controller channels, but that
does not establish four external motor channels. This agrees with board issue I7.

Required design: one reviewed connector/pin map for four motor channels and J7
(HB, HALT, UART TX/RX, grounds), with explicit signal direction/polarity and
inactive bias. Resolve the old active-high physical_fault inputs instead of
leaving obsolete inputs floating. Add synchronized HB capture, initial 50 ms
absence detection, sticky fault, explicit clear conditions and observable status.
Define cold boot, never-seen HB, static high/low, fast/noisy edges, resumed HB,
clear-versus-expiry priority, and what survives CORE_RESET.

Acceptance: RTL fault injection, generated ABI/driver checks, full board build,
CDC review and connector-level measurement. Old route results cannot qualify
the additional I/O or new logic.

### R3 — High: recovery can become circular; UART authority is contradictory

Evidence: DMOTOR_CONTROL §6.4 stops HB with a latched fault and requires monitor
acknowledgment/HB restoration before FPGA acknowledgment. Monitor FIRMWARE §5
latches HALT and requires HALT release, Zynq acknowledgment and fresh ARM to
rearm. Once HB loss drives a latched outward HALT, a literal implementation can
wait forever: monitor waits for HALT release while PL waits for healthy HB.
The documents also call UART advisory while requiring a UART acknowledgment
for rearm. `Runtime::prepare()` currently performs local GPIO release and three
acknowledgment writes; it has no monitor exchange or long-lived recovery states.

Required design: a joint transition table separating sensor health, local-fault
acknowledgment, outward HALT, HB qualification, physical ARM and motion enable.
Specify whether healthy SAFE may emit HB while permits remain low, and how a
peer HALT differs from an unresolved local sensor fault. Every transition must
name preconditions, owner, timeout and failure result. Recovery must retain zero
motion intent and cannot be an automatic retry loop.

Recommended UART policy: loss during operation is advisory under the existing
decision; loss during an explicit recovery exchange may refuse recovery. State
that distinction. Specify framing, baud, size limit, CRC parameters, byte order,
version, boot/session identifiers, sequence/replay handling, units, validity,
timeouts and duplicate acknowledgment semantics. CRC alone is not freshness.
Test both boot orders, reset of either processor, held ARM, delayed/replayed ACK,
lost UART and simultaneous HALT/HB expiry.

### R4 — High: PL clock-loss shutdown is not established by the new topology

DMOTOR_ARCH §11 requires clock-loss inhibition outside stopped-clock logic.
The monitor watchdog watches ESP32 HB, not PL progress. If the PL clock stops
with PWM high and HALT low, a healthy ESP32 can continue HB and permits while
current remains below its configured limits. PL authorization/target timers
also stop. This is a design failure scenario, not a measured board result.

Required decision: provide independently supervised evidence of PL progress
that can revoke monitor permits, or explicitly narrow the supported failure
model. Using UART progress as a protective lease would change its present
advisory-only contract and needs an explicit architectural decision. A Linux
process heartbeat alone would not prove that the PL control frame advances.
Test actual control-clock loss separately from CPU stalls and ESP32 hangs.

### R5 — High: the ESP32 10 ms safety cycle lacks a total execution bound

Evidence: FIRMWARE §3–4 assigns all acquisition to one 10 ms task but gives
each I2C transaction a 2 ms timeout. Four sensors each require freshness/status
and measurement reads. Even two timed-out transactions per sensor would consume
16 ms before ADC/state processing. No total cycle budget or early-abort rule is
specified. Three consecutive failed cycles also does not define the immediate
permit/HB response during the first two failures.

Required design: inventory normal and worst-case transactions, use an absolute
cycle deadline, stop further bus work once the budget is exhausted, and explicitly
define permits/HB on transient errors, stale samples and overruns. Do not toggle
HB after a late cycle merely because it eventually completed. Reset the task
watchdog only after the specified successful progress point. Establish a bound
from last valid sensor conversion to permit revocation, distinct from a 25 ms
sample-age threshold and the 50 ms PL HB timeout.

The proposed 588 µs shunt/bus conversions with four averages yield about 4.7 ms
updates, consistent with the [INA226 datasheet](https://www.ti.com/lit/ds/symlink/ina226.pdf).
However, reading Mask/Enable clears conversion-ready and can clear latched alert
evidence. Define a single owner/read order that records flags before clearing,
and advance freshness only on evidence of a new conversion. Do not wire
conversion-ready alerts into the fault ISR without distinguishing their purpose.
Test bus held low, timeout on each transaction, identical successive readings,
sensor reset, simultaneous ALERT sources and partial population.

### R6 — Medium: core pinning is not sufficient isolation for NVS/ISR timing

Evidence: FIRMWARE §5 defers NVS persistence to telemetry on core 1, but that
alone does not bound interference with core 0. Flash/cache behavior depends on
the IDF build and module; IRAM-safe ISRs require both code and accessed data to
remain available. See [Espressif flash concurrency](https://docs.espressif.com/projects/esp-idf/en/v5.4.3/esp32s3/api-reference/peripherals/spi_flash/spi_flash_concurrency.html).

Required design: pin IDF/toolchain/sdkconfig, use static resources for the safety
path, audit ISR dependencies and cross-core locking, and prohibit calibration,
flash persistence and firmware updates while armed unless specifically qualified.
Persist fault records only after inhibition; use bounded RAM records first.
Implement the 500 ms task watchdog with the millisecond API, check setup results,
and configure panic/reset explicitly; the Kconfig timeout is expressed in seconds.
See [Espressif watchdog API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/wdts.html).
Verify total reset-to-permits-low latency, including panic handling, on the pinned build.

### R7 — Medium: commissioning diagnostics can block the Linux control loop

Evidence: `software/apps/daemon.cpp:trace()` captures telemetry, writes four CSV
rows and flushes stdout synchronously. It runs before `runtime.tick()` and on
commands, including powered commissioning. A full pipe or stalled output sink
can therefore delay publication, watchdog refresh, command HALT and the one-second
host pulse deadline. Hardware timeouts remain a backstop, but the host deadline
is not a hard maximum pulse duration under that condition.

Required change: bounded nonblocking diagnostic enqueue with a drop counter;
separate output draining from the executor. Process due protective deadlines
before diagnostics. State nominal pulse duration and independent maximum fault
response separately. Test an unread stdout pipe, slow storage, CPU pressure and
client flood; measure refresh gaps and HALT latency on the target.

### R8 — Medium: sensor/configuration rules are not yet a complete contract

FIRMWARE §4 requires all six ADC channels while I4/I5 require absent motor inputs
to be ignored. §5 uses approximate plausibility windows; §7 names NVS thresholds
without schema, ranges or corruption/version behavior. Stall thresholds/duration,
startup allowances, hysteresis, current filtering and ADC sample-age handling
remain unspecified. “Any non-solid-low” STOP is an electrical requirement, not
a third value available from a binary GPIO read.

Required design: battery channels always mandatory, motor channels selected only
by a validated population mask; missing expected hardware blocks arming. Define
calibrated units, finite/range checks, defaults, schema/version/integrity checks,
atomic update behavior and invalid-NVS refusal to arm. Calibration is inhibited
and cannot silently remove a failed sensor from the population. Validate pack B
by calibrated full-minus-mid with error bounds. Give each threshold persistence,
clear hysteresis and timestamp semantics. Qualify STOP input voltage margins and
use immediate unsafe assertion with delayed, deliberate release.

### R9 — Medium: architecture documents describe incompatible board revisions

DMOTOR_ARCH §12 still includes a battery INA226 and omits the added watchdog/HALT
gate. DMOTOR_CONTROL includes five ALERTs, a removed battery monitor, old joined
balance taps, a MOSFET rather than the implemented 2N3904, and firmware-only HALT
claims alongside the newer hardware path. Its current-limit target also differs
from the later issue register. Monitor README contains both “No PCB exists yet”
and routed-PCB status. These are consequential inputs to firmware, not cosmetic
history: they change expected devices, recovery behavior and protection limits.

Required cleanup: a dated current-system contract tied to the ordered fabrication
revision, plus clearly separated historical evidence. Reconcile to the actual
board intent and assembly options; do not silently choose resistor values from
conflicting text. Preserve the user's existing uncommitted edits while doing so.

### R10 — Planned gap: vehicle runtime and release qualification remain open

The current daemon is a maintenance/commissioning service, not the normal vehicle
planner. DMOTOR_SW Phase 5 remains unimplemented. Its maintenance lease must not
be reused as an unstated autonomous-trajectory policy. Define command ownership,
planner progress, localization age/loss, coordinate frames, fractional encoder
conversion, reset epochs, bounded queues and completion-after-STOP before adding
position moves. Retain global rearm semantics until independently qualified.

Deployment must bind software, bitstream/ABI, monitor firmware, calibration and
board revision into one reproducible manifest. Boot/restart/update must leave
intent empty and outputs inhibited. Existing native/QEMU evidence does not
establish real MMIO ordering, scheduler bounds or electrical shutdown.

## Recommended implementation sequence

1. Reconcile the current board contract and settle R1–R4 jointly. Deliver the
   pin map, output truth table, boot/recovery transition table and UART contract.
2. Implement a host-compilable ESP32 safety core with explicit timestamped inputs
   and desired permit/HB outputs. Add a thin IDF acquisition/ISR adapter; only
   the safety owner may authorize outputs. Resolve R5/R6/R8 in its detailed design.
3. Add PL monitor integration and driver/runtime recovery operations. Build a
   joint fault-injection harness exercising both state machines before hardware.
4. Remove blocking Linux diagnostics; measure executor timing and retain existing
   mailbox/timeout regressions. Add tests for the failure cases in this review.
5. Bring up unpowered outputs and sensors, then qualify end-to-end protection on
   the actual boards. Proceed to bounded single-motor, four-motor and loaded tests
   only with the corresponding electrical/current-limit gates satisfied.
6. Add the normal vehicle planner and qualify deployment/recovery. Freeze the
   cross-device interface after the integrated tests, not before discovering
   required recovery/status fields.

## Validation performed for this review

Fresh native build in `/tmp/dmotor-review-20261103`:

```sh
cmake -S software -B /tmp/dmotor-review-20261103 -DDMOTOR_SW_SANITIZE=ON
cmake --build /tmp/dmotor-review-20261103 -j4
ctest --test-dir /tmp/dmotor-review-20261103 --output-on-failure
```

Result: **8/8 passed**, UBSan enabled: semantics, protocol, Linux inspection,
runtime, powered, direct-duty, commissioning and service tests (8.87 s total).
Service tests use fake hardware. This pass does not cover the absent ESP32
firmware, the monitor electrical path or the joint recovery contract. No hardware
was programmed and no HLS/Vivado, ARM/EDF or physical tests were rerun.
