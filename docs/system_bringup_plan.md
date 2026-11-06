# Integrated Zynq and ESP32 development and bring-up plan

**Status: IMPLEMENTATION STARTED — 2026-11-04; technical gates remain open.** Baseline inspected: dmotor `63d5cdb`.
This amended proposal incorporates the supplied plan review and retains the seven
bring-up stages B1–B7, with a separate optional vehicle-development workstream BV.
It does not approve powered operation, adopt the proposed recovery contract, or
claim completion of hardware tests. B1–B7 and BV are integrated-plan identifiers;
they must not be confused with the older PS-only Phases 0–6 in `DMOTOR_SW.md`.
The inspected implementation baseline remains `63d5cdb`; the review document
revision is its Git commit (`git log -1 -- docs/system_bringup_plan.md`).

Review alongside the [documentation consolidation plan](documentation_consolidation_plan.md).
The first extraction/archive increment is recorded in its manifest; remaining
migrations and document acceptance are tracked under G1d/U10. After adoption, maintain current state, decisions
and unresolved work directly instead of appending successive historical narratives.

## 1. Objective, boundaries and baseline

Deliver a reproducible system in which Zynq PL controls motor timing, Zynq PS owns
intent and application lifecycle, and the ESP32-S3 monitor owns sensor protection,
permits and physical ARM/STOP. Demonstrate safe inhibition and deliberate recovery
under the adopted failure model before expanding to loaded vehicle operation.

The authoritative implementation/evidence matrix is [system_status.md](system_status.md).
[decisions.md](decisions.md) owns active choices; [open_issues.md](open_issues.md)
owns unresolved work. The [reviewed baseline snapshot](archive/pre-system-plan-2026-11-04/docs/system_bringup_plan.md)
retains the prior tables and their revision identity.

First release is commissioning-only; BV/GV follows later. Robert Klinkhammer owns
gate acceptance using time-separated self-review. Technical contracts and gates
remain subject to their explicit acceptance criteria.

## 2. Gate management and evidence

Each gate has designated **roles** below, not a requirement for that many people.
Assign a named accountable owner and reviewer before executing its acceptance tests;
one person may cover multiple subsystem roles. Record which roles each person covers.
For a one-person project, use a separate, time-separated self-review of the recorded
checklist and raw evidence, and disclose that it is not independent review. For G5,
G6 and GV powered qualification, select a named external/second reviewer or explicitly
adopt a documented self-review substitute before testing; an unassigned review mode
leaves the gate open. A second operator is still required wherever the adopted test
procedure requires one; review substitution does not waive operational controls.
The accountable owner records acceptance or rejection and any restrictions. A failed
gate returns to the affected workstream. No automatic advancement.

For each gate record: requirement/issue IDs, exact source revisions and dirty state,
build/toolchain hashes, board/harness revisions, test setup and initial state,
stimulus, expected result and limit, actual measurements, raw evidence location,
pass/fail, reviewer/date and any explicitly accepted restriction. Keep a gate open
if a required measurement, threshold, reviewer or artifact identity is missing.

| Gate | Required result | Primary roles |
|---|---|---|
| G1a | Recovery/admission contract, cleanup proof and shared software interfaces | PS + ESP32 + PL |
| G1b | Electrical/inhibited test envelope and conditional R4 experiment/disposition | Hardware + PL + system |
| G1c | Firmware acquisition/configuration budgets, threshold policy and SDK isolation design | ESP32 + hardware |
| G1d | Documentation ownership, initial traceability matrix and migration rules | Documentation + subsystem reviewers |
| G2 | Documented inhibited electrical/boot/IO bring-up | Hardware + PL/PS + ESP32 |
| G3 | Tested monitor firmware and safe startup behavior | ESP32 + hardware |
| G4 | Joint recovery/admission implementation and adversarial simulation pass | PS + ESP32 + PL |
| G5 | Integrated failure/timing measurements satisfy adopted limits | All subsystem owners + test operator |
| G6 | Bounded powered motor commissioning accepted | Hardware + controls/PS + operator |
| GV | Vehicle planner and loaded vehicle behavior accepted, if full-vehicle scope selected | PS + controls + hardware/operator |
| G7 | Reproducible matched release, deployment/recovery procedure and evidence | Build/release + all subsystem reviewers |

Work may start at specific sub-gates rather than waiting for all of G1:

- G1a unblocks recovery/core state-machine design and B4 simulator/coordinator work.
- G1b unblocks B2 inhibited electrical bring-up and R4 characterization.
- G1a + G1c unblock the complete B3 monitor implementation; G2 is additionally
  required for its target acceptance G3. Early state-machine tests may start at G1a.
- G3 plus completed B4 implementation permits G4 target acceptance; G2/G3/G4 and
  the accepted test limits are prerequisites to G5.
- G5 plus the approved powered procedure permits G6. A commissioning-only release
  follows G6 → G7; a full-vehicle release requires G6 + GV → G7.
- BV requirements/software/simulation may run in parallel once its own entry
  contracts are accepted; loaded vehicle tests wait for G6 and the BV software gate.
- G1d starts immediately with a minimum requirements-to-test matrix. Full archive
  migration proceeds in parallel and must finish before G7, not before B2–B4 coding.
  Relevant acceptance rows and current contracts must exist before each test runs.

Fixture-only inhibited diagnostic images may support G2 before full monitor firmware
exists. They must be distinctly identified and must not grant operational ARM authority.
G1 is a planning group, not an all-or-nothing barrier or a blanket approval to power motors.

## 3. B1 — Resolve outstanding contracts and consolidate documentation

**Entry:** current ordered-board sources, wiring, code and review findings available.

- Adopt/amend recovery v4 as one cross-device contract. Specify output/permit state,
  command admission, cleanup proof, freshness and uncertainty behavior for every
  transition; determine any remaining PL observability/ABI changes explicitly.
- Select a conditional R4 candidate: independent PL-progress supervision or an
  explicitly bounded narrower supported failure model, to be tested before final
  adoption. Distinguish clock stop, reset, unconfigured fabric,
  reconfiguration and power loss. No XDC-only fix or HB-based inference closes R4.
  Identify any required rev-A rework, test fixture, harness change or new board.
- Finish the absolute monitor acquisition budget, timeout/early-abort rules, sensor
  freshness semantics, HB success point and watchdog interaction. Identify normal
  and worst-case I2C/ADC/ISR execution and shared-resource costs.
- Define population mask, mandatory battery taps, calibrated units, integrity/version
  checks, calibration update atomicity, missing-sensor policy and invalid-data
  refusal to arm. Define every protection threshold, persistence, hysteresis,
  startup allowance and source timestamp, including imbalance separately from LVC.
- Review ESP-IDF v6.1 on ESP32-S3 for the actual GPIO/ISR/flash/watchdog requirements.
  The environment pin is settled; firmware SDK suitability and isolation are not.
- Adopt documentation ownership and perform the extraction/link audit in the
  [consolidation plan](documentation_consolidation_plan.md). Separate historical
  implementation status from current decisions before moving files.

**First deliverable:** seed the [requirements-to-test matrix](system_requirements_matrix.md)
with stable IDs, owning contracts, limits/TBD blockers and planned evidence. Expand
it before implementation acceptance; do not leave traceability until archive completion.
Other deliverables are the separately reviewed contracts, candidate R4 test plan,
budget/threshold tables and documentation migration manifest.

**G1a acceptance:** recovery transitions, software interfaces and cleanup proof required
by B3/B4 are adopted or amended; no undefined transition is used in implementation.

**G1b conditional acceptance:** approve only the inhibited test envelope, candidate
R4 disposition and experiments below. U01 remains open; this is not R4 closure.

**G1c acceptance:** acquisition/configuration rules and implementable budgets are
reviewed. Values still TBD explicitly block their affected target/powered tests;
fixture-only development cannot silently choose operational thresholds.

**G1d acceptance:** document owners, extraction rules and initial matrix are reviewed.
Full migration completion is a G7 deliverable and need not block independent coding.

### Conditional R4 decision and rev-A rework triggers

Before G1b, record the candidate failure response, safe test setup, observed signals,
expected outputs/bounds, abort criteria and reviewer for each experiment. In B2,
characterize clock stop (including a frozen active output), core/fabric reset,
unconfigured/reconfigured fabric and each relevant rail-loss/power sequence with
motor drive disconnected. Observe PWM/PH, coast, permits and DRVOFF; account for
measurement changes caused by removing power from the observed driver itself.
In B5, repeat with the implemented monitor, final candidate circuit and matched
software, including simultaneous failures required by the chosen model.

If an in-scope failure leaves drive possible beyond its approved bound, startup
levels are unsafe, or an independent monitor cannot detect the required progress
loss, reopen the candidate decision. Review independent supervision, rev-A rework/
harness changes or a board revision. If the required protection cannot be qualified
on rev A, block that powered scope or adopt an explicit narrower scope with its
excluded failures and rationale before G5. No automatic scope narrowing is allowed.
Repeat affected G2/G3/G4 checks after changes; G5 requires a final measured disposition.
An unchanged output under frozen-clock conditions is not proof of safe inhibition.

## 4. B2 — Bring up boards with motor drive power disconnected

**Entry:** G1b conditional acceptance for wiring/electrical/test scope; known inhibited fixture;
reviewed board revisions and harness drawings. Preserve physical STOP during planned
FPGA reconfiguration; software HALT may disappear when fabric is reconfigured.

- Inspect assembly, polarity, rails and harness continuity against ordered sources.
  Check shared grounds, 3.3 V logic, connector mapping and absence of back-powering.
  J7 has no supply connection; Pmod/motor harnesses are mapped, not straight-through.
- Test physical STOP and permit/DRVOFF paths before relying on firmware. Measure
  safe/inactive levels at boot, reset and power sequencing; record unresolved cases.
- Boot the matched EDF image. Identify kernel, PL capabilities, driver binding,
  UART node and software source hash. Verify UART Lite is enabled and not a console
  or getty; PS console remains separate. Check exclusive device ownership/HALT.
- Flash a distinctly identified inhibited ESP32 diagnostic build once available:
  permits initialize low, no arm path. Verify board GPIO map, mandatory battery
  channels, populated motor sensors, I2C addressing, ADC scaling and fault inputs.
- Execute the G1b R4 characterization experiments; record results and reopen the
  candidate disposition where required, without energizing motors.
- Verify encoders manually and synthetic signals with drive disconnected. Check
  direction/count mapping, channel independence, PWM/PH/HB/coast logic and UART
  framing with a scope/logic analyzer. Record encoder sign/count/CPR mapping under
  SYS-19 before using it for commissioning. Internal drive_enable is diagnostic only.

**Deliverables:** board/harness checklist, rail/logic measurements, encoder mapping,
UART loop/exchange evidence, correct software/hardware identities and defect list.

**G2 acceptance:** expected channels and protection wiring verified, no unexplained
energizing path, initial permit-low behavior measured, and all unresolved startup/
PL-failure cases tracked under the G1 failure model. This gate does not authorize motors.

### Bench software to implement before its tests

Use the [bench diagnostic program plan](bench_diagnostic_plan.md) as the concrete
implementation list: first the Zynq Linux UART utility and ESP32-S3 UART-only
image, then inhibited GPIO/HB modes. The dedicated PL clock-stop variant and
isolated core-reset support are later, independent tasks. Existing UART transport
and host simulations are building blocks, not ready-to-run two-board diagnostics.
Full operational ESP32 firmware is not a prerequisite for the UART pair.

For this hobby project, use the [G1b bench record](gates/G1b.md): reference existing
board revisions/drawings, note the actual hookup and save useful logs/captures.
Robert's dated second look at the setup is sufficient; no separate operator,
signature package or duplicate board inventory is required. Missing software
blocks its experiment, not unrelated bench work. Keep motor drive power
disconnected and preserve the meaningful electrical checks.

### Serial link checkpoint

Test the physical Zynq–ESP32 serial connection during **B2/G2**, after G1b
conditional approval of the inhibited wiring/test envelope. Do not wait for full
recovery firmware. G1b is currently open; the inhibited ESP32 diagnostic build
and target evidence must be supplied before this checkpoint can be completed.

1. Keep motor drive power disconnected and physical STOP asserted. Confirm the
   diagnostic ESP32 image initializes permits low and has no ARM path. Verify
   rails, shared ground and 3.3 V signal levels before connecting the harness.
2. Check the reviewed harness mapping: Zynq TX CK_IO0/T14 to monitor J7-5 RX;
   monitor J7-4 TX to Zynq RX CK_IO1/U12; J7-1/6 ground. J7 carries no supply.
   Confirm the actual board revision against the harness drawing.
3. Boot the matched PL/kernel/device-tree image. Identify the UART Lite device
   from its binding/address (0x42C00000), not a guessed tty number. Confirm
   115200 8N1 at both ends, no flow control, and no console/getty or competing
   process using that device. Keep the PS console separate.
4. With a bounded diagnostic sender/receiver at each end, send known counted
   patterns in both directions, including 0x7d/0x7e and maximum-size protocol
   frames. Record sent/received counts, mismatches, CRC/framing errors and analyzer
   captures. Separate local loopback from the actual two-board exchange; loopback
   alone does not establish the harness/peer connection. Diagnostic tools must
   not treat traffic or HELLO as permission to arm.
5. Repeat the exchange after each endpoint resets and after UART cable removal
   and reconnection within the approved test envelope. Record image hashes,
   device node, pins, configuration, durations, counts and permit-low observations.
   Stop and resolve unexplained corruption, back-powering or permit transitions.

This checkpoint establishes basic electrical/transport operation. G3/G4 then
qualify the actual monitor/coordinator protocol, durable sessions and recovery;
G5 tests system fault timing and UART-loss/reset behavior. Host codec, replay and
scripted exchange tests are useful prerequisites, but do not replace this physical
test or authorize motor drive power.

## 5. B3 — Implement and qualify ESP32 safety firmware

**Entry:** G1a + G1c for complete firmware implementation; G1a alone permits
early state-machine tests. G2 hardware evidence is required for target acceptance.

- Implement a host-testable C safety core under `software/esp32/components/`, with
  explicit time/sample inputs and outputs. Cover BOOT/SAFE/ARM_PENDING/ARMED/FAULT,
  fault latching, recovery epochs, held ARM, STOP and population/calibration rules.
- Implement ESP-IDF board adapters: safe GPIO initialization, sensor acquisition,
  ISR-class inhibit, level polling, bounded scheduling and freshness tracking.
  Stop bus work at the cycle deadline; do not emit HB for late/invalid cycles.
- Integrate the existing monitor schema/CRC vectors through a C-compatible adapter
  or generator output rather than creating a second hand-maintained protocol.
  Finish cause bits, endpoint-session persistence and fault-event representation.
- Implement task/external-watchdog servicing at the defined progress points; test
  failure paths. Audit ISR call/data placement, critical sections and cross-core
  dependencies. Use bounded RAM fault records; prohibit flash/NVS/calibration/
  updates while armed unless separately designed and qualified.
- Add firmware CMake/sdkconfig defaults/partitions, pinned component dependencies,
  version reporting, native tests and CI to the separate ESP-IDF build.

**Deliverables:** monitor firmware, host tests, firmware configuration/calibration
schema and tooling, measured acquisition/ISR/watchdog/startup evidence.

**G3 acceptance:** safe GPIO precedes peripheral/task startup; invalid calibration,
missing required sensors, stale/over-budget cycles and injected stalls produce the
specified permits/HB response. Measured timing satisfies the approved G1 bounds.
Firmware cannot arm merely because it booted or sees heartbeat/UART activity.

## 6. B4 — Complete PS–ESP32 recovery and application admission

**Entry:** G1a protocol/cleanup contract; monitor simulator available. G3 is
required for final target acceptance; software work can run alongside B3.

- Implement persistent endpoint session allocation and explicit discovery/pairing/
  re-pairing; handle allocator failure and identifier exhaustion without reuse.
- Add PS recovery admission hold to the daemon/runtime. Discard stale queued intent,
  establish verified disabled/reset/drained PL state and a fresh session, then
  build attestation from proof, not unconditional success flags.
- Execute the acyclic cleanup → exchange → physical ARM/query sequence. Release
  HALT only at the specified contract step while motion admission remains held;
  holding the coast line throughout would recreate the recovery deadlock.
- Bind every exchange to peer sessions, recovery epoch, transaction and sequence.
  Handle duplicates, changed-content duplicates, stale/reordered traffic and new
  faults between messages. Resolve lost COMMIT_ACK through bounded state queries;
  never infer SAFE versus ARMED from an absent reply.
- Open admission only after a fresh matching ARMED query and explicit recovery
  completion. Reconnect, monitor reset, old ARM presses or healthy HB never replay
  previous targets. Initial boot arming also requires the exchange.
- Integrate monitor UART into the daemon with bounded work and independent deadlines.
  Preserve the client library boundary; version daemon/client interfaces together
  if recovery status or commands need new fields. Existing `prepare()` is PL-only
  until this work explicitly replaces its semantics.
- Replace blocking executor diagnostics with bounded enqueue/drop accounting and
  separate draining; process protective deadlines before optional reporting.

**Deliverables:** PS coordinator, ESP32 peer, shared deterministic simulator,
updated typed client/status API where required, runtime and protocol tests.

**G4 acceptance:** both boot orders; reset shorter than HB timeout; held ARM;
new faults in every state and between messages; lost ACK; stale/replayed/mutated
frames; sequence/session exhaustion; UART disconnect; queued intent during hold;
and failed cleanup all satisfy the contract. No automatic restart or circular wait.

## 7. B5 — Qualify integrated faults and timing, still inhibited

**Entry:** G2–G4 accepted; instrumentation and test firmware identified; R4 test
implementation or adopted restriction explicitly available.

- Observe PWM/PH, COAST_REQ, physical permits and driver DRVOFF together. Measure
  PS HALT, independent DISABLE, controlled STOP, monitor ISR faults, sensor timeout,
  HB expiry and watchdog paths separately. Do not substitute GPIO readback for scope.
- Reset/stall PS and ESP32 separately, disconnect/corrupt UART, suppress or distort
  HB, break/hold I2C, remove expected sensors and corrupt calibration. Verify no
  stale drive on recovery and no reset required merely to make a failed test pass.
- Test PL clock loss, fabric/core reset, reconfiguration and rail loss separately
  with synthetic/no-power outputs. Record the behavior of frozen active signals
  as well as undriven signals under the adopted R4 design/failure model.
- Exercise unread stdout, slow storage, CPU load and client flood. Measure executor
  gaps, permit-clear/HALT latency, admission hold, lease expiry and maximum output
  duration. The nominal one-second software pulse is not a measured hard bound.
- Record recurring errors and recovery repetitions, not just one successful boot.
- Review and update the existing [powered-commissioning procedure](../software/linux/powered-commissioning.md)
  for this integrated system. Its location is `software/linux/`, not `docs/`.
  Record physical-STOP access and pre-test check, battery isolation/connection/
  disconnection and fault handling, current protection, restrained mechanical setup,
  operator/spotter duties, bystander exclusion, abort criteria and post-test safe
  state. Validate the setup without drive power; obtain named procedure approval
  before any B6 test. Existing procedure text alone is not integrated approval.

**Deliverables:** fault-injection matrix, traces, worst-case timing report and
closed/accepted issue dispositions linked to exact software and board revisions,
and a reviewed integrated powered-test procedure as the B6 entry deliverable.

**G5 acceptance:** every required fault reaches the specified inhibited state within
its approved bound and recovers only through the approved sequence. Any missed
bound, unexplained permit restoration or unsupported failure class blocks the
corresponding powered scope. No unrestricted powered acceptance with R4 unresolved.

## 8. B6 — Commission powered motor operation

**Entry:** G5 for the intended operating scope; reviewed current limits, mechanical
setup, physical STOP access, motor/driver ratings and validated configuration;
approved integrated [powered procedure](../software/linux/powered-commissioning.md)
from B5, with operator/battery/bystander controls and review mode assigned.

1. Begin with bounded single-motor direct duty under that approved powered procedure.
   Confirm direction, current, fault response and measured pulse/inhibit limits.
2. Commission closed-loop velocity per motor: CPR, gains, acquisition, settling,
   zero-speed behavior, STOP deadlines and current/stall policy. Save calibration
   and configuration identity; do not weaken sensor protection to achieve a pass.
3. Expand to simultaneous channels, load changes and selected STOP/disable while
   peers run; verify supply interactions, global faults and global rearm effects.

**Deliverables:** motor profiles, measured single-/multi-motor commissioning records
and the accepted motor operating envelope.

**G6 acceptance:** each power/load expansion has its own passing record and meets
approved motor requirements. G6 does not accept vehicle planning or loaded vehicle
behavior. A commissioning-only release may proceed to G7 with that restriction.

### BV — Separate vehicle-development workstream (full-vehicle scope only)

**Entry:** full-vehicle scope selected; G1a admission contract and reviewed BV
requirements for coordinate frames, command/queue ownership, localization freshness,
planner-stall behavior, wheel geometry and measurable completion/error limits.
Software and simulation may start before G6; no loaded vehicle acceptance occurs early.

- Design and implement trajectory/position/dwell, kinematics/odometry and external
  pose handling as a separate PS deliverable with explicit command state and deadlines.
- Use `dmotor_client` for all daemon calls; evolve daemon/client schemas together
  where needed. Do not use the commissioning lease as an unreviewed product policy.
- Test queues, cancellation, admission hold, stale pose, planner stall and recovery
  in simulation. Define distance/heading/overshoot/settling limits before tests.
- Record a software-readiness review before powered vehicle work. Then, after G6,
  qualify loaded motion, localization/communications loss, stopping and deliberate
  recovery under a vehicle-specific approved extension of the powered procedure.

**Deliverables:** planner interfaces/design, implementation, simulation suite,
software-readiness review and loaded vehicle acceptance evidence.

**GV acceptance:** reviewed vehicle requirements and measured loaded behavior pass;
all G6 prerequisites and the vehicle test procedure remain satisfied. GV is required
for a full-vehicle G7 release. If commissioning-only is selected, mark BV/GV explicitly
out of release scope and retain U08 as deferred, not resolved.

## 9. B7 — Release, deployment and maintenance qualification

**Entry:** G1a–G1d and G2–G6 accepted, with the final G5 R4 disposition recorded;
GV is additionally required for a full-vehicle release. G1d acceptance covers the
initial mapping and migration rules, not completed migration. Remaining archive
migration is a B7 work item and must finish before G7 acceptance. No unresolved
issue is silently excluded from the release scope.

- Build matched PL/XSA/device tree, ARM kernel/driver/daemon/client packages and
  ESP32 firmware from recorded revisions. Re-run native and target tests; include
  client SDK packaging and monitor wire compatibility checks.
- Record dmotor/edf-dev/esp32-dev/hardware revisions, image/toolchain digests, PL ABI
  and capabilities, source hashes, ESP-IDF/config/partition identities, protocol,
  board/harness revisions and calibration generation. Produce checksummed artifacts.
- Reproduce deployment on a clean system; test boot ordering, permissions, service
  lifecycle, missing/incompatible components, restart and ownership loss. Boot
  must leave outputs inhibited, without automatically preparing or enabling motion.
- Define inhibited update/rollback, FPGA reconfiguration with physical STOP held,
  calibration migration/refusal, log retrieval and recovery from interrupted updates.
  Rollback must preserve protocol compatibility and session-freshness guarantees.
- Complete the documentation migration and its requirement/issue mapping, link and
  staged-input checks under G1d/U10; verify that the current index leads to the
  accepted release documents and historical evidence remains traceable.
- Archive released artifacts/evidence, current restrictions and reviewer acceptance;
  freeze released interfaces and document any subsequent migration explicitly.

**G7 acceptance:** another operator can reproduce, identify, deploy and recover the
matched release from the published procedure. All acceptance records are attached;
remaining restrictions are explicit, and documentation migration has passed its
acceptance checks. Build success alone is not a gate sign-off.

The Zynq build entry point remains:

```sh
cd ~/workspace/edf-dev
EDF_DMOTOR_SOURCE=../dmotor ./edf --target arty-z7-20 build
```

This builds artifacts; it neither completes ESP32 firmware nor qualifies or arms
the physical system. ESP32 firmware is built separately through `esp32-dev` once
its application exists in dmotor. Keep detailed commands in the owning build guides.

## 10. Unresolved issue register for review

The current [U01–U11 issue register](open_issues.md) is maintained separately.
Its owner/disposition updates must not be duplicated in this plan.

## 11. Review decisions and first execution slice

Reviewers should explicitly disposition:

- Confirm the execution baseline; first-release scope is now commissioning-only (D01).
- G1a–G1d assignments and the conditional R4 candidate/experiments/rework triggers.
- Recovery v4 semantics, required proof/ABI, timing/threshold owners and gate limits.
- The archive/current-document split and migration inventory.
- Robert Klinkhammer is the named gate owner/reviewer (D02); assign operational
  duties, board availability and instrumentation, and approve the integrated powered procedure.

After adoption, produce the initial traceability rows and assign G1a–G1d owners.
Close each applicable sub-gate independently: start inhibited B2 characterization
at G1b, recovery simulation at G1a, and complete monitor implementation at G1a/G1c.
Run archive extraction in parallel; do not let a wholesale documentation move stall
software development or postpone traceability. If full-vehicle is selected, schedule
BV independently and require GV before its release. Do not rewrite completed PL
integration or create a second firmware/protocol implementation.

Unaccepted contracts and gate criteria remain reviewable proposals even when committed. Record review comments
against their commit hashes; a commit establishes identity, not gate approval.
