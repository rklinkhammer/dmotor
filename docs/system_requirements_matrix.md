# Initial system requirements-to-test matrix

**Validation update (2026-11-08):** [G2 operator evidence](gates/G2.md#pre-pcb-acquisitionuart-results--2026-11-08)
now covers 10,000 successful concurrent acquisitions, 1,000 verified UART replies,
400/400 delayed-injection expirations, 400 I/O-only failures without overruns,
link-loss timeouts and explicit restored-link PASS. These are diagnostic subcase
passes, not acceptance of G1c/G3 or SYS-03–06. Sensor freshness/calibration, loaded
policy/output work, isolation and electrical shutdown remain open. See the
[prioritized implementation/validation plan](system_status.md#next-implementation-and-validation).

> Architecture update (2026-11-07): main uses the [1 ms PS motor executor](../DMOTOR_ARCH.md) with independent PL capture/PWM/protection and a matched PSF1 driver/daemon. Client contracts, commissioning-first scope, U0 evidence, the hardware pause and pending live recovery are unchanged.

**Status: DRAFT FOR REVIEW — 2026-11-04.** Seed matrix for the
[B1–B7/BV system plan](system_bringup_plan.md), implementation baseline `63d5cdb`.
This is the first G1d deliverable, not a complete requirements decomposition or
acceptance report. Robert Klinkhammer is the accountable owner/reviewer, using time-separated
self-review. Specific operational duties still need assignment per test case. No row is closed
by this document, by a test count, or by successful compilation.

Before a row's acceptance test runs, assign its owner/reviewer, split compound rows
into atomic test cases, specify stimulus/preconditions and measurable expected
results, and attach the approved contract revision and numeric limits. A TBD limit
blocks that acceptance test; it does not prevent a separately approved inhibited
characterization experiment from discovering the needed data.

| ID | Requirement / owning source | Review owner role | Verification and evidence to attach | Blocking gate / unresolved input |
|---|---|---|---|---|
| SYS-01 | Saved connector/pin mapping, voltage levels and direction; [Arty interface](arty_monitor_interface.md) | Hardware + PL | Harness continuity, rail/back-power checks; PL TX T14 → monitor RX and monitor TX → PL RX U12; scope traces | G2; approved G1b fixture and electrical limits |
| SYS-02 | Safe startup and separate PL clock/reset/configuration/power failures; [recovery contract §1](recovery_transition_contract.md#1-authority-and-physical-behavior) | Hardware + PL + system | G1b candidate experiment records in B2; final B5 frozen-active/undriven output traces, rework and retest records | G1b conditional; U01 remains open until final G5 scope/bounds disposition |
| SYS-03 | Permits low before firmware initialization and outside authorized monitor state; recovery contract §§1–2 | ESP32 + hardware | Reset/boot-order/state tests plus physical permit/DRVOFF measurements | G3/G5; U02/U04, approved startup/response limits |
| SYS-04 | HB only after fresh, valid, on-budget monitoring work; recovery contract §§1,5 | ESP32 | Absolute acquisition budget; I2C timeout/stale-data/overrun tests; timestamped HB/permit traces | G1c/G3/G5; U03, exact budget/age/detection limits |
| SYS-05 | Independent task/external watchdog and ISR/flash isolation; recovery contract §5 | ESP32 | ISR dependency audit; task stall, cache/flash and watchdog recovery measurements | G3/G5; U04, approved watchdog/reset latency limits |
| SYS-06 | Valid population/calibration and complete protection policy; [ESP32 plan](../software/esp32/README.md) | Hardware + ESP32 | Missing expected sensor/battery tap, corruption/version, calibration interruption and threshold boundary tests | G1c/G3/G6; U06, threshold/persistence/hysteresis/error limits |
| SYS-07 | Verified PL cleanup, no stale motion admission and acyclic recovery; recovery contract §§2–3 | PS + PL + ESP32 | Simulator transition coverage; attestation/readback proof; queued intent, cleanup failure and new fault at every transition | G1a/G4/G5; U02, CLN1 implemented and actual-HLS tested; joint admission/attestation qualification remains |
| SYS-08 | Session/epoch/transaction freshness and bounded uncertainty; recovery contract §§3–4 | PS + ESP32 | Both boot orders, rapid restart, allocator failure, exhaustion, replay/mutated duplicate/reordering, lost COMMIT_ACK and query exhaustion | G4/G5; U02, adopted protocol and reply/query deadlines |
| SYS-09 | Physical ARM is deliberate and cannot reuse stale eligibility; recovery contract §§2–3 | ESP32 + PS | Held ARM, fault during ARM, epoch change and fresh ARMED-query admission tests; physical switch/permit traces | G4/G5; U02/U06, switch qualification and timing |
| SYS-10 | STOP, DISABLE and global HALT retain distinct output behavior; [programming guide](controller_programming.md) | PL + PS + hardware | Selected STOP with peers running, immediate DISABLE, direct-duty STOP, global coast; PWM/PH/coast/permit/DRVOFF traces | G5/G6; approved command-to-inhibit, stop/settle and coast limits |
| SYS-11 | Sole owner, typed client use, explicit lease and no retry/reconnect resume; [client contract](../software/CLIENT.md) | PS | Native client/service tests, target process loss/timeout/malformed traffic and ownership checks; exact run logs | G4/G5/G7; board lease/HALT qualification; existing native evidence is not target evidence |
| SYS-12 | Diagnostics cannot block protective scheduling; system plan B4/B5 | PS | Unread stdout, slow storage, CPU pressure and client flood; drop counts, worst executor/refresh gaps and inhibit latency | G4/G5; U05, approved scheduling and worst-case output-duration limits |
| SYS-13 | Powered tests use an approved integrated procedure; [existing powered procedure](../software/linux/powered-commissioning.md) | Hardware + operator | Reviewed battery/STOP/mechanical/bystander/abort checklist; dry run, named operators/review mode and per-expansion records | G6 entry; U11, G5 and integrated procedure approval |
| SYS-14 | Powered motors meet their configured envelope; system plan B6 | Controls + hardware + PS | Single-/multi-motor current/direction/velocity/settling/fault measurements and profile identities | G6; calibrated motor/current/stall/STOP limits, no weakening protection for a pass |
| SYS-15 | Vehicle planner has independent software and loaded qualification; system plan BV | PS + controls | Reviewed frames/queues/ownership, pose freshness and planner-stall requirements; simulation, software-readiness review and loaded vehicle tests | GV after G6 for loaded tests; U08; explicitly deferred for commissioning-only release |
| SYS-16 | Reproducible matched release and safe deployment/rollback; system plan B7 | Build/release + subsystem reviewers | Exact manifests/hashes, ARM packages/client SDK, ESP32 build, clean deployment, interrupted update and rollback/freshness records | G7; U09; GV additionally required for full-vehicle scope |
| SYS-17 | Current documentation has one owner per requirement and preserves historical evidence; [archive proposal](documentation_consolidation_plan.md) | Documentation + subsystem reviewers | Original-to-successor manifest, hashes, requirement/issue mapping, generator/staging/link checks | G1d initial mapping; U10 migration may finish during B7 but must pass before G7 acceptance |
| SYS-18 | Initial timing policy requires target validation: 100 ms authorization, 20 ms refresh, 100 Hz target publication, 100 ms target timeout and 500 ms client lease; active technical decision 7 in [decisions.md](decisions.md) and runtime policy | PS + PL | Verify configured values, then measure target publication/application and authorization-refresh intervals under nominal/load conditions; independently stop refresh, target publication and client activity and record expiry/inhibit traces. Link scheduler stress evidence from SYS-12 and ownership-loss evidence from SYS-11 | G5 before G6; approved tolerances, observation points and worst-case detection/inhibit limits required. Nominal constants alone do not establish those limits |
| SYS-19 | Encoder channel, direction, count scale and CPR rational must match physical motor mapping; system plan B2 and motor configuration | Hardware + PL/controls | With drive disconnected, rotate each shaft in both directions for a recorded number of output revolutions; record per-channel signed count delta, channel independence, quadrature decode factor, gearbox ratio and resulting counts_numerator/counts_denominator. Use synthetic inputs to exercise decoder limits separately; link measured mapping to each motor profile | G2 mapping before SYS-14/G6 commissioning; expected sign convention, reference revolutions, count tolerance and effective CPR must be reviewed; provisional datasheet counts are not acceptance evidence |

For each case derived from these rows, record:

```text
Requirement ID / case ID:
Approved contract revision and acceptance limit (or TBD blocker):
Named implementer, reviewer, and review mode:
Artifact/board/harness/calibration identities:
Initial state, stimulus, instrumentation and abort conditions:
Expected result / measured result / raw evidence path:
Pass, fail or blocked; issue and retest links:
Reviewer disposition and date:
```

The small-team review substitute is defined in system-plan §2. It does not turn
self-review into independent assurance or remove a test procedure's operator needs.
Review the matrix at G1a–G1d and before G3–G7/GV acceptance. Expand it as contracts
settle; keep prior run evidence immutable and link superseding runs rather than
rewriting old measurements.

## Monitor follow-up implementation — 2026-11-08

[Implementation and validation record](monitor_next_steps.md) adds explicit receiver
rejections, physical-device freshness/configuration checks, calibration integrity,
16 ms recovery simulation and a serialized safety-core candidate with a
hard-inhibited task adapter. U03–U06 and G1a/G1c/G3–G5 remain open: no calibrated
operational application, sustained conversion-age proof, ISR/watchdog qualification
or new electrical hardware result is asserted. The one-INA six-read path and
four-INA 24-read path require new timing evidence. G1b/G2 are not closed by builds.

## Consolidated physical-health disposition — 2026-11-08

[Current G2 matrix](gates/G2.md#physical-ina-health-results--2026-11-08) records
one-INA nominal PASS (one warm-up + 99 good; max 3,918 us) and wrong-address
rejection PASS (100 io failures; max 739 us). The updated receiver reason test now passes with explicit `failure` and 400
injected failures without skips/expirations. Expected-register mismatch is
operator-reported completed elsewhere; raw-evidence linkage remains pending. These diagnostic subcases advance
U03/U06 evidence, not SYS-03–06 or electrical acceptance. G1a/G1b/G1c/G2 remain
open; G3–G6 and G7 remain unaccepted. Commissioning-first scope, 1 ms PS loop,
16 ms monitor/25% reserve and assembled-board pause are unchanged.
[Next implementation and validation](system_status.md#next-implementation-and-validation).
