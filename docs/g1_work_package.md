# G1 execution work package

**Current scheduling (2026-11-06):** Further physical testing is paused until
assembled monitor and motor boards are available, at Robert's request ([D04](decisions.md)).
Completed U0 results remain valid; U1 is implemented/build-tested, with no physical
U1 acceptance recorded. Procedures below are retained for resumption.

Status: in progress; Robert Klinkhammer is accountable owner/reviewer, using
separate preparation and self-review records. This document makes the first
[system-plan](system_bringup_plan.md) contract work concrete; it grants no gate
acceptance, hardware-test permission or motion authority.

## G1a — Recovery and admission proof

The [reset/cleanup audit and admission specification](g1a_cleanup_admission.md)
now traces A1–A4/A8. It identifies lazy context initialization as a barrier to
claiming all-context cleanup from reset readback. An adapter reset regression
passes; an internal disabled four-context cleanup primitive and native reset tests
are implemented. Combined mailbox cleanup, CLN1 MMIO completion, kernel access and PS verification
are integrated in source. Actual motor-HLS co-simulation, full AXI and interrupted-cleanup integration
now pass; the matched revision-11 hardware/ARM build also passes. Time-separated
review and remaining recovery qualification are outstanding. A5–A7 and G1a acceptance remain open.

The [single proposed recovery contract](recovery_transition_contract.md) remains
v4. Its wire layout exists in `software/protocol/monitor/wire.json`; PS framing
and UART transport exist. A successful decode or `Runtime::prepare()` is not
proof of completed recovery. Complete these tasks before G1a acceptance:

| Task | Existing implementation/evidence | Required deliverable and acceptance |
|---|---|---|
| A1: recovery admission hold | [Runtime hold boundary](g1a_runtime_admission.md) host-tested; [coordinator/ticket integration](g1a_recovery_coordinator.md) host-tested; real socket queue integration remains open | State/command table for every public Runtime/client operation during hold; queued targets discarded; only explicit completion opens admission; simulator tests tied to SYS-07/09 |
| A2: outputs disabled | STATUS bits 0–3; driver/MMIO path and runtime HALT | Specify repeated/race-aware disabled verification across cleanup and completion; tests inject new fault/enable attempts; no single stale read treated as perpetual proof |
| A3: reset/drain/PI cleanup | CLN1 disabled sweep/completion plus session/request binding; actual-HLS reset interruption and full controller tests pass; see [evidence](g1a_cleanup_admission.md#8-combined-cln1-integration) | Trace reset into mailbox/HLS/PI state and outstanding AXI/HLS results; derive minimum assertion/completion conditions and prove with RTL tests. Review CLN1 completion semantics and complete the remaining boundary/race cases. CORE_RESET readback alone does not prove drain |
| A4: session/configuration/causes | [Runtime attestation](g1a_cleanup_attestation.md) checks session/configuration/CLN1 and local/HB state with mid-read mutation tests; atomic hardware races remain open | Map each attestation flag to exact reads, ordering, reset guarantees and failure exits; verify expiration/new fault wins ACK and no unverified flag is sent |
| A5: endpoint sessions | [PS journal allocator](g1a_endpoint_sessions.md) implemented and host-tested in isolation; ESP32 backend and deployment policy remain open | PS/ESP32 storage format, integrity/atomic-update strategy, power-loss/exhaustion handling and inhibited allocation ordering; never reuse a lifetime identifier |
| A6: pairing/freshness | [Symmetric link candidate](g1a_session_link.md) host-tests HELLO confirmation, restart, duplicate caching and sequence checks; connected through the isolated coordinator, not the live daemon | Specify HELLO confirmation, explicit re-pair inhibition, duplicate cache and sequence advancement; cover stale/mutated frames and peer restart in deterministic tests |
| A7: outcome uncertainty | [Joint model/coordinator tests](g1a_recovery_coordinator.md) cover COMMIT_ACK loss and challenged STATE_REPLY | Transition table for lost ACK, SAFE/ARM_PENDING/ARMED responses, changed epoch/session and bounded retry/query exhaustion; preserve hold on ambiguity |
| A8: observable client semantics | [Read-only client recovery status](g1a_client_status.md) and compatibility tests pass; Client.prepare remains PL-only | Decide explicit recovery/status API and wire versioning, client admission/error behavior and how UI reports pending physical ARM; do not silently relabel prepare success as full recovery |

**Model increment:** the [post-pairing host model](g1a_recovery_model.md) now exercises
admission hold, challenged ARM, lost ACK and bounded UNKNOWN handling. Its design
choices remain for review. It is disconnected from deployed motion admission.
The [PS allocator candidate](g1a_endpoint_sessions.md) now supplies the host-tested
A5 storage primitive. The [A6 link candidate](g1a_session_link.md) now supplies
isolated pairing/replay checks. The [wire adapter](g1a_recovery_link.md) now joins these to the model with a
scripted two-endpoint exchange test. The [behavioral simulator](g1a_monitor_simulator.md) now exercises fault epochs,
staged expiry and physical ARM gestures through the real codec. The [Runtime admission boundary](g1a_runtime_admission.md) now blocks held motion
and discards buffered targets. The [coordinator](g1a_recovery_coordinator.md) now checks retained owner/generation
tickets in joint tests. The [MMIO attestation path](g1a_cleanup_attestation.md) now acquires the proof
with repeated identity/configuration/protection reads. [Durable PS reservation](g1a_session_integration.md)
now runs before PL preparation with allocation failures tested while inhibited.
The [A8 read-only recovery status API](g1a_client_status.md) is implemented; live
mutation/admission remains behind its explicit review/proof checklist. Finish A5
target provisioning/ESP32/storage qualification and complete
A2–A5/A8 proof and integration before connecting the model to live admission. This does not
create an alternate protocol or relax any live daemon precondition.

Acceptance record: link the amended/adopted v4 revision, A1–A8 dispositions and
SYS-07/08/09 cases. Required observability must be demonstrated or explicitly
implemented before attestation code is accepted; do not populate cleanup flags
with unconditional `7` as a substitute for proof.

## G1b — Inhibited R4 experiment design

Complete the [editable G1b record](gates/G1b.md) for the operator/reviewer.
It now references the recorded rev-A hardware and uses a short hobby-bench
checklist. The [bench diagnostic program plan](bench_diagnostic_plan.md) identifies
the Linux/ESP32 UART pair, GPIO/HB modes and later PL/reset injection work; missing
software blocks only its dependent experiment.

U01 is open. Candidate choices remain independent PL-progress supervision versus
an explicitly restricted supported failure model; no choice is adopted here.
The experiment design must precede its execution. Motor drive power stays
disconnected and the approved fixture must remain inhibited independently of PL.

| Case | Observe | Decision informed |
|---|---|---|
| B1: control-clock stops with active output | PWM/PH/coast/permit/DRVOFF; distinguish frozen level from undriven pin | Can independent hardware detect the required loss of progress? |
| B2: CORE_RESET versus fabric reset | Same signals plus reset/HB; separate each reset domain | Which guarantees survive which reset? |
| B3: configuration absent/reloaded | Safe levels and supply sequence; physical STOP held | Are external bias/rework and update restrictions required? |
| B4: individual rail loss/restoration | Rail voltage, back-power paths, signals while observer/driver powered | Does a low observed voltage merely reflect an unpowered observer? |

Before running, fill in exact fault-injection method, permitted rails/levels,
instrument grounding/reference, a sketch/photo of any wiring changes, abort
actions, and expected response (or a characterization question). Robert can record
these in the bench checklist; no duplicate design dossier is required.
Record traces against SYS-02; an experiment is not closed by “no movement” with
motor power absent. Review whether results require rev-A rework/harness changes,
a new board or a restricted scope; do not silently exclude a failed case.

## G1c — Monitor budget and configuration closure

The [G1c design tables and pinned-IDF audit](g1c_monitor_design.md) are now prepared
from rev-A sources. Actual population, thresholds and timing measurements remain
for review. The tables cover:

- Acquisition inventory: channel/device, mandatory versus population-selected,
  transaction count, normal/worst-case duration, conversion timestamp, deadline,
  early-abort behavior and permit/HB action on stale/failed/over-budget data.
- Protection policy: calibrated units, valid range, threshold, persistence,
  hysteresis, startup allowance, cause bit, response bound and inhibited clear rule.
  Battery taps stay mandatory; imbalance policy is distinct from LVC.
- Calibration storage: schema/version/integrity, atomic update, corruption behavior,
  error budget and inhibited-only persistence. Missing calibration blocks arming.
- Isolation audit: ISR call/data placement, locks/shared resources, flash/cache/NVS
  constraints, watchdog API/config and measured reset-to-permits-low behavior.

No operational thresholds are selected by this work package. The 10 ms cycle,
ISR/staleness/LVC targets in v4 remain proposed until derived and reviewed. The
ESP-IDF v6.1 tool image is pinned; that does not qualify the firmware's isolation.
Link the completed tables to SYS-03–06 and U03/U04/U06 before G1c acceptance.

## G1d — Current records and first archive increment

Implemented: separate current state/decisions/issues; selected release scope and
review ownership; 19-row seed matrix; snapshot manifest with exact originals for
the reviewed plan and chronological review/PL/PS/build records; current navigation.

Remaining: Robert's time-separated extraction review; atomic test-case expansion;
remaining mixed documents and companion inventories; complete requirement mapping,
link/packaging audit and eventual G7 migration acceptance. Historical component
documents remain reachable from the archive index pending extraction. The first
increment does not mark the whole archive migration complete.
