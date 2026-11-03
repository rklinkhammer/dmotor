# Zynq and ESP32 software architecture review

Date: 2026-11-03. Boards are ordered; this review treats the current rev A
hardware as the integration target. Recommendations below are proposed changes,
not newly approved requirements or evidence of physical qualification.

**Latest review — PROPOSED v2, 2026-11-03:** checked dmotor `b02267b`
(clean working tree before this review; v2 is committed, not uncommitted).
The v2 assessment below supersedes the v1 follow-up's closure status. V2 makes
substantial progress, but the claim that F1–F5 are all resolved is not supported.
No RTL, driver or monitor implementation accompanies the proposal.

## Latest assessment: recovery contract v2

| Finding | Credit for v2 | Remaining status |
|---|---|---|
| F1 | Recovery exchange required before permits, including after monitor boot | Partially addressed: epoch binding and continuous inhibition are underspecified |
| F2 | Latch clearing no longer depends on exchange; final ACK and framing added | Original circular dependency resolved in the design; commit failure/replay rules remain open |
| F3 | Withdraws the claim that rev A inherently asserts coast before configuration | Open: replacement XDC mitigation and exposure-bound claims are incorrect or unsupported |
| F4 | Global emergency coast separated from selected-motor controlled STOP; lifecycle mapped | Original STOP/global-coast conflict resolved in the design; clarify immediate DISABLE semantics |
| F5 | Explicit HB qualification prerequisite, held-ARM rule and separate detection classes | Partially addressed: state coverage and timing bounds remain incomplete |

R1/R2 still require implementation and qualification. R3 remains open for the
protocol issues below. R4 remains an open system requirement even though it is
outside this contract's scope. R5–R8/R10 are unchanged; residual R9 cleanup may
follow adoption, but deferment is not closure. The following checks concern the
actual v2 text, not the summary of its intended behavior.

### V2-1 — High: bind recovery authorization to an epoch and retain inhibition

The monitor returns `epoch` in RECOVER_STATE, but COMMIT contains only `txn` and
no rule binds that transaction to the captured epoch. Define that binding and
invalidate pending transactions and completed ARM eligibility on every new
fault, peer/session change and boot. Otherwise a delayed pre-fault COMMIT can
be mistaken for recovery of a newer fault within the same boot session.
Initialize boot with **no completed exchange**, even when epoch starts at zero.
T7 currently covers only ARMED; faults during SAFE, ARM_PENDING and the exchange
must also invalidate authorization and win over COMMIT/permit writes.

The PS attestation is a snapshot, not a sustained interlock. Keep the PL disabled,
targets/PI state cleared and command admission closed from cleanup through the
exchange and physical ARM; define the explicit transition that admits a new
command afterward. Reject old queued intent. A monitor reboot interlock does not
cover a PL-only reboot while monitor permits remain high: define that case
separately rather than claiming A′ bounds it. Test new faults between STATE and
COMMIT, faults during ARM_PENDING, repeated recovery, and commands arriving after
attestation but before ARM.

### V2-2 — High: COMMIT outcome and session validation remain contradictory

The monitor enters SAFE when it sends COMMIT_ACK. If that ACK and all retries are
lost, the later claim that timeout leaves the monitor in FAULT is false: the PS
and monitor disagree about the result. Choose explicit outcome-reconciliation
rules. Do not allow ARM against an exchange the PS abandoned while assuming its
peer remained inhibited. T8 also requires stages 1–3 to enter SAFE, but stage 3
is physical ARM and T5 requires SAFE first; change T8 to the intended exchange
completion boundary so that the transition table itself is acyclic.

`peer_session_echo` validates the receiver's identity only. It does not reject a
packet from an old **sender** boot when the receiver has not rebooted. Require
`src_session` to match the established peer and specify discovery/session-change
handling. Bind txn to both sessions and the monitor epoch; define transaction
reuse, sequence-window advancement and duplicate-cache expiry. The two-bitfield
identities being present in a frame is not the same as validating both of them.
The v2 wire description also needs explicit byte order for multibyte fields
other than CRC and bounded decoded/escaped frame lengths.

Implementation note: `DMOTOR_RELEASE_HALT` already exists in the kernel driver,
and `Runtime::prepare()` already calls `io_.halt(false)`. Extending it to a new
PL latch may need work, but describing release as a wholly new driver operation
misstates the current implementation. Three acknowledgment writes alone do not
clear all targets/PI state; map cleanup to the existing reset/session sequence
and verify completion before attestation.

### V2-3 — High: XDC PULLUP is not a pre-configuration safety mechanism

V2 B′ proposes an XDC PULLUP as asserting COAST_REQ before configuration. That
claim must be withdrawn: configuration-time pull behavior is controlled by
PUDC_B and power sequencing, not a user-design XDC setting alone. AMD recommends
an external pull-up when immediate power-ramp assertion is critical; see
[7 Series configuration guide UG470](https://docs.amd.com/api/khub/documents/FOs3lXmlcWxBhTIFxVKyGA/content).
Verify the Arty/Zynq board configuration and supply states before selecting a
mitigation. The quoted 2.6 V unloaded divider estimate also cannot establish
Q2 operation: its base-emitter junction loads that node; validate available
base current and the resulting ENABLE voltage instead.

A′ imposes no elapsed-time bound on stale permits after PL-only failure; a
future recovery is not a shutdown deadline. Remove “bounds the stale-permit
window” and “stationary-safe” as unsupported guarantees. Distinguish CORE_RESET,
fabric reset, reconfiguration, clock loss and power loss. EN/PH pulldowns govern
undriven pins, not every actively driven or frozen-output case. Keep this a
documented open hardware/system issue, with no claim of a one-line fix.

### V2-4 — Medium: the revised timing/state rules still need derivation

T7b's 30/40 ms figures do not close R5: three stale cycles occur only after the
25 ms age threshold, and the 10 ms cycle itself lacks a worst-case bound.
Battery low-voltage policy also has a 500 ms persistence interval. Name the
starting event for each bound and separate persistence, detection and output
latency. A STOP sample checked by a task is not automatically an ISR event.
Keep proposed targets separate from measured guarantees.

The scoped zero-intent invariant still says **every** inhibit/fault transition
clears intent, contradicting A′'s admitted inability to clear PL intent on a
monitor-only fault. Express separate invariants for monitor permits and PL
cleanup. Define HB_QUALIFIED invalidation and fault precedence, not just its first
assertion. Finally, per-motor DISABLE must remain immediate output inhibition;
“zero-target + EN=0” must not become a zero-velocity PI command or controlled STOP.

**Follow-up validation:** source/document comparison and vendor documentation
check only. No executable changes, test rerun, FPGA build or hardware experiment.
The initial 8/8 test result later in this file is historical evidence only.

**Follow-up review, 2026-11-03:** includes dmotor `f983070` and
motor-interface-integrated `21133df`, particularly the new
[proposed recovery transition contract](recovery_transition_contract.md).
R1/R3 now have a proposed design, but remain open for the reasons below.
R9 is partially resolved by the board-document reconciliation. No implementation
or physical qualification was added by these updates.

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

## Follow-up status and review of the proposed recovery contract

This section records the **v1** review. Use the latest v2 assessment above for
current closure status; the original findings are retained for traceability.

| Finding | Updated status |
|---|---|
| R1 — software HALT/coast | Open: outward COAST_REQ proposed; reset bias, STOP scope and implementation unresolved |
| R2 — board PL integration | Open: qualifier timings proposed, but no pin map/RTL/ABI implementation |
| R3 — joint recovery | Open: useful liveness/permission split and UART policy; transition/protocol defects below |
| R4 — PL clock loss | Open: a combinational coast request cannot detect a frozen clock by itself |
| R5 — safety-cycle bound | Open: acquisition budget and first/second failure behavior unchanged |
| R6 — ESP32 isolation | Open: IDF/flash/ISR/watchdog implementation contract unchanged |
| R7 — Linux diagnostics | Open: synchronous trace output unchanged |
| R8 — sensor/configuration contract | Open: population, NVS and threshold details unchanged |
| R9 — document consistency | Partially resolved: listed board facts corrected; residual contradictions remain |
| R10 — vehicle/release | Planned work unchanged |

The proposal makes three useful distinctions: monitor liveness versus permission,
outward coast request versus local fault attribution, and advisory operational
telemetry versus required recovery messages. These address the original design
questions, but are not sufficient to close R1/R3. The following are contract
defects to resolve before adopting or implementing it.

### F1 — High: healthy HB can conceal a monitor fault from the PL

Section 1A allows HB during FAULT while UART remains advisory in operation.
For a monitor-detected fault with valid/fresh samples (for example nFAULT or a
current threshold), permits drop but the PL receives no mandatory fault signal.
It may retain an active velocity target and PI state until a separate feedback
or acquisition fault occurs; that secondary detection is not the monitor-fault
contract. The current core-reset/watchdog mechanisms do not prove intent was
invalidated at the instant the monitor revoked permission.

Specify how every monitor fault invalidates PL intent before permits can be
restored. A recovery interlock must prove a fresh PL session, disabled outputs
and cleared targets even when UART was absent throughout the fault. Define
boot/restart the same way: a quick monitor reset may not exceed the 50 ms HB
window, and T5/T6 currently allow physical ARM without a fresh peer handshake.
Test a short reset and a monitor fault with UART disconnected and a still-live
host target. Neither ARM nor HB resumption may reconnect stale drive.

### F2 — High: T10 and the exchange still contain a circular dependency

T10 requires a recovery exchange to clear LOCAL_FAULT if COAST_REQ was asserted.
Exchange step 1 instead requires the PS to have already cleared its latches
under T4/T10. T8 also requires HALT_IN deasserted, but a retained LOCAL_FAULT
holds COAST_REQ asserted. The new HB policy removes one circular wait, not this
one. Define staged recovery with separate authorization to clear local latches,
release COAST_REQ and commit monitor SAFE, while motion stays disabled throughout.
Specify PS deliberate-HALT release explicitly; it is absent from the table.

Step 3 also lacks a final monitor acknowledgment: if COMMIT is lost, PS cannot
know the monitor entered SAFE. Bind every exchange to both peer boot identities
and a transaction identifier; specify final ACK, replay rejection, sequence wrap,
duplicate handling and timeout reconciliation. A single sender session and
16-bit sequence do not by themselves reject a delayed command from an earlier
peer boot. Complete framing rules (delimiter escaping/resynchronization), CRC
reflection/final-XOR/wire order and payload definitions before protocol tests.

### F3 — High: proposed reset-time asserted bias is not present on rev A

Section 1B/T2/T11 assume COAST_REQ is asserted while PL is unconfigured.
The ordered monitor circuit intent has R17 from Q2 base to GND; the active PL
HALT output constraints use PULLDOWN. Monitor README/I10 explicitly say an
absent FPGA does not inhibit. An RTL expression cannot establish an output
level before configuration or with FPGA power absent.

Separate the desired reset guarantee from the as-ordered hardware behavior.
Document and qualify any external bias/rework needed, or revise the guarantee
and recovery preconditions. Do not treat T11's peer observation as established
for every reset/power case. This also leaves R4's frozen-output case open.

### F4 — High: global coast conflicts with controlled and selected-motor STOP

Section 1B asserts COAST_REQ for STOP/DISABLE, but the truth table omits those
conditions and the single Q2 gate inhibits all four motors. Immediate assertion
on STOP prevents the controlled deceleration required by DMOTOR_ARCH §7.
A selected-motor STOP/DISABLE would also coast peers whose runtime targets and
watchdogs may remain active. Define whether coast occurs after STOP completion,
whether these operations become global, and how peer intent is invalidated.
These are behavior changes, not just a rename of shared_halt.

Clarify READY after STOP as well: the proposal allows READY braking but requires
STOP coast. A distinct retained coast/rearm state or explicit latch is needed
to express both. The proposed PL RESET/READY/RUN shorthand must map to the
existing DISABLED/READY/RUNNING/STOPPING/HALTED lifecycle.

### F5 — Medium: transition guarantees need state-specific conditions

T3 allows one second for the first HB while §2 permits PWM in READY/RUN whenever
fault latches are clear. Add an explicit HB-qualified prerequisite before any
motion authorization; an initial grace period must never authorize motion.
T7 claims every cause was inhibited in an ISR, but stale samples and battery
thresholds are task-detected. Give those paths their own bounds. ARM press versus
release, a held button across recovery, and STOP-cycle qualification also need
precise event definitions. The blanket invariant that every row leaves zero
intent cannot describe ordinary ARMED operation; scope it to inhibit/recovery
transitions and state exactly when fresh motion intent becomes admissible.

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

**Follow-up:** the implementation finding still holds. Proposed contract §1B
addresses the intended outward signal; F3/F4 above block design closure.

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

**Follow-up:** the following evidence describes the original contract, still
retained pending adoption. The new proposal explicitly separates UART roles
and permits healthy HB in FAULT; F1/F2/F5 above describe the remaining gaps.

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

**Partially resolved in the follow-up.** DMOTOR_ARCH §12 now removes the battery
INA226 and includes the watchdog/HALT gate. DMOTOR_CONTROL revision 3 identifies
the ordered rev A baseline, corrects four ALERTs, the single-conductor balance
taps and divider ratios, Q1's 2N3904 type, and the provisional current-limit
description. Monitor README's stale “No PCB exists yet” sentence is removed.

Remaining: DMOTOR_CONTROL §1 still describes FPGA coast as firmware-mediated,
and §6.1 says only driver protection/passive topology are software-independent,
despite the added watchdog/Q2 gate. Its old §6.4 HB/fault semantics are explicitly
pending replacement, not resolved by a link to a PROPOSED contract. Reconcile
those statements once F1–F5 are settled. Monitor README also retains pre-release
wording about physical fit despite the later issue-register clearance. Close
R9 only after current behavior and historical statements are clearly separated;
this documentation closure would not close physical qualification findings.

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

1. Revise the proposed recovery contract to resolve F1–F5, then settle R1–R4
   jointly. Complete the pin map, output truth table and wire protocol; reconcile
   the residual R9 text to the adopted behavior and actual ordered hardware.
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

The results below are from the initial review, not a rerun after the documentation
update. The follow-up compared the two repositories' document changes, current
runtime/RTL wiring and monitor circuit intent. No executable changes were made;
native tests were not rerun for this documentation-only follow-up.

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
