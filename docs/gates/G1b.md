# G1b record — inhibited electrical and PL-failure test plan

**Status: DRAFT — incomplete; NOT ACCEPTED. No experiment results recorded.**

Edit this file in place. Replace `TBD` with a value or a linked drawing/procedure.
Use `N/A — reason` where appropriate; do not leave an applicable field blank.
Check boxes only after the corresponding item is actually reviewed or verified.
Sections 1–7 are the G1b planning/review record. Section 8 is for later B2 results.

G1b conditionally approves the inhibited test envelope and R4 experiments. It does
not close U01/R4, accept G2, or authorize motor drive power. G1a completion is not
an entry requirement; the fixture must not rely on unfinished recovery software.

References: [G1 work package](../g1_work_package.md),
[system plan](../system_bringup_plan.md), [requirements matrix](../system_requirements_matrix.md),
[open issues](../open_issues.md), [wiring reference](../arty_monitor_interface.md).

## 1. Record identification


| Field                                                 | Entry                                                |
| ----------------------------------------------------- | ---------------------------------------------------- |
| Record revision / preparation date                    | 11/05//2026                                          |
| Prepared by                                           | Robert Klinkhammer                                   |
| Test operator                                         | Robert Klinkhammer                                   |
| Accountable owner / reviewer                          | Robert Klinkhammer                                   |
| Review method                                         | Time-separated self-review against recorded evidence |
| Preparation completed timestamp                       | TBD                                                  |
| Review timestamp                                      | TBD — must be separate from preparation             |
| Reviewed repository commit and any additional changes | TBD                                                  |
| Release scope                                         | Commissioning-only first; vehicle work follows       |
| Requirements / issue                                  | SYS-02; U01 / R4; add other affected IDs: TBD        |

## 2. Hardware, harness and software baseline

Record the actual articles to be tested, not only their intended design names.


| Item                                          | Board revision / serial or identifier | Drawing, source revision or artifact                 |
| --------------------------------------------- | ------------------------------------- | ---------------------------------------------------- |
| Zynq / Arty                                   | Artifact                              | TBD                                                  |
| ESP32 monitor board / module                  | TBD                                   | TBD                                                  |
| Motor interface boards and populated channels | TBD                                   | TBD                                                  |
| Test fixture / independent inhibit circuit    | TBD                                   | TBD                                                  |
| Motor harness / connector mapping             | TBD                                   | TBD                                                  |
| Monitor harness / connector mapping           | TBD                                   | TBD                                                  |
| Proposed rev-A rework or temporary wiring     | TBD, or none                          | TBD                                                  |
| PL image / fault-injection variant            | TBD                                   | Hash and build manifest: TBD                         |
| PS image / diagnostic tools                   | TBD                                   | Hash and build manifest: TBD                         |
| ESP32 diagnostic image, if used               | TBD                                   | Hash; permit-low initialization and no ARM path: TBD |

- [ ]  Harness drawings match the actual connector pin mapping and signal direction.
- [ ]  Supply and ground connections are identified, including J7 having no supply pin.
- [ ]  Any diagnostic/injection image is distinguishable from the operational image.
- [ ]  Required fixture/diagnostic functionality is available, or explicitly blocks its experiment.

## 3. Inhibited test envelope


| Field                                                                      | Entry                                                     |
| -------------------------------------------------------------------------- | --------------------------------------------------------- |
| Motor drive power disconnection points and verification method             | TBD                                                       |
| Motor/load connections permitted during these tests                        | TBD                                                       |
| Independent inhibition mechanism, schematic and verification method        | TBD — must not depend on PL progress or PS software HALT |
| Physical STOP connection and use during configuration/reset                | TBD                                                       |
| Energized rails: nominal voltage, allowed range and current limit for each | TBD                                                       |
| Rails prohibited from being energized                                      | TBD                                                       |
| Power-up, power-down and restoration sequence                              | TBD                                                       |
| Permitted signal levels and interface limits                               | TBD                                                       |
| Back-power paths to check, including instrumentation                       | TBD                                                       |
| Scope/analyzer/probe models, ratings and channel mapping                   | TBD                                                       |
| Instrument grounding/isolation and reference nodes                         | TBD                                                       |
| Rail and signal measurement points / drawing                               | TBD                                                       |
| Pre-run inspection and continuity checklist                                | TBD                                                       |
| Immediate shutdown/abort procedure and responsible operator                | TBD                                                       |
| Allowed activities in this envelope, including any UART test               | TBD                                                       |
| Explicitly excluded activities                                             | Powered motor operation; additional exclusions: TBD       |

- [ ]  Motor drive power is excluded by the documented setup.
- [ ]  Inhibition survives the PL failures being injected.
- [ ]  Instrument references and probe connections are reviewed for the actual setup.
- [ ]  Limits, shutdown actions and restoration steps are specified before execution.

## 4. Proposed R4 disposition

Select and describe one candidate for evaluation; neither option is pre-approved.

- [ ]  Independent PL-progress supervision.
- [ ]  Explicitly restricted supported failure model.


| Field                                                            | Entry        |
| ---------------------------------------------------------------- | ------------ |
| Candidate design / restriction and rationale                     | TBD          |
| In-scope failures and combinations                               | TBD          |
| Excluded failures, consequences and rationale                    | TBD, or none |
| Expected failure detection and inhibition path                   | TBD          |
| Detector's independent clock, power and reset assumptions        | TBD          |
| Required rev-A rework, fixture, harness change or board revision | TBD, or none |
| Evidence needed before final adoption                            | TBD          |

A frozen output is not evidence of inhibition. Heartbeat inference or XDC bias
alone does not establish protection against all clock/reset/configuration/power
failures. U01 remains open after G1b conditional acceptance.

## 5. Experiment worksheets

Complete every applicable field. Split a case into separately identified subcases
where fault injection, rail state or expected behavior differs. For a
characterization-only case, state the question to answer rather than inventing a
pass limit. Electrical limits and abort conditions are still required.

### B1 — Control-clock stop with an active output


| Field                                                                            | Entry                                                                                                 |
| -------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------- |
| Objective                                                                        | Distinguish a frozen driven output from an undriven pin; evaluate independent progress-loss detection |
| Exact clock/domain and injection mechanism                                       | TBD                                                                                                   |
| Initial output state, how established and verified with drive power disconnected | TBD                                                                                                   |
| Numbered execution and restoration procedure / attachment                        | TBD                                                                                                   |
| Observe                                                                          | PWM, PH, coast, permits, DRVOFF and relevant clock/progress indication                                |
| Probe points, trigger, sample rate and capture duration                          | TBD                                                                                                   |
| Expected states and response bound, or characterization question                 | TBD                                                                                                   |
| Abort conditions and action                                                      | TBD                                                                                                   |
| Repetitions / initial states / operator                                          | TBD                                                                                                   |
| Planned evidence file names / location                                           | TBD                                                                                                   |

### B2 — CORE_RESET versus fabric reset


| Field                                                                      | Entry                                                |
| -------------------------------------------------------------------------- | ---------------------------------------------------- |
| Objective                                                                  | Establish which guarantees survive each reset domain |
| Separate reset sources/domains, assertion duration and injection mechanism | TBD                                                  |
| Initial states, energized rails and physical STOP state                    | TBD                                                  |
| Numbered execution and restoration procedure / attachment                  | TBD                                                  |
| Observe                                                                    | PWM, PH, coast, permits, DRVOFF, reset and HB        |
| Probe points, trigger, sample rate and capture duration                    | TBD                                                  |
| Expected states/bounds for each reset, or characterization questions       | TBD                                                  |
| Abort conditions and action                                                | TBD                                                  |
| Repetitions / operator / planned evidence files                            | TBD                                                  |

### B3 — Configuration absent and reloaded


| Field                                                                     | Entry                                                                            |
| ------------------------------------------------------------------------- | -------------------------------------------------------------------------------- |
| Objective                                                                 | Establish configuration-time levels and required bias/rework/update restrictions |
| Method for unconfigured state and configuration reload                    | TBD                                                                              |
| Rail sequence, initial states and physical STOP held throughout procedure | TBD                                                                              |
| Numbered execution and restoration procedure / attachment                 | TBD                                                                              |
| Observe                                                                   | PWM, PH, coast, permits, DRVOFF and relevant configuration/rail signals          |
| Probe points, trigger, sample rate and capture duration                   | TBD                                                                              |
| Expected levels/bounds, or characterization questions                     | TBD                                                                              |
| Abort conditions and action                                               | TBD                                                                              |
| Repetitions / operator / planned evidence files                           | TBD                                                                              |

### B4 — Individual rail loss and restoration


| Field                                                                   | Entry                                                                                      |
| ----------------------------------------------------------------------- | ------------------------------------------------------------------------------------------ |
| Objective                                                               | Distinguish actual inhibition from an unpowered driver/observer; identify back-power paths |
| Each rail to remove/restore and exact injection mechanism               | TBD                                                                                        |
| Other energized rails, retained observer power and initial states       | TBD                                                                                        |
| Numbered execution and restoration procedure / attachment               | TBD                                                                                        |
| Observe                                                                 | Rail voltages, back-power indicators, PWM, PH, coast, permits and DRVOFF                   |
| Probe references, trigger, sample rate and capture duration             | TBD                                                                                        |
| Expected states/bounds per rail/sequence, or characterization questions | TBD                                                                                        |
| Abort conditions and action                                             | TBD                                                                                        |
| Repetitions / operator / planned evidence files                         | TBD                                                                                        |

## 6. Rework and scope-change triggers


| Observation                                                         | Required disposition / owner                                            |
| ------------------------------------------------------------------- | ----------------------------------------------------------------------- |
| An in-scope failure leaves drive possible beyond the approved bound | Stop affected scope; reopen R4 candidate; owner: TBD                    |
| Startup or configuration levels violate the approved envelope       | Stop affected scope; review bias/rework/update restrictions; owner: TBD |
| Required progress loss cannot be independently detected             | Review supervision, rev-A rework or board revision; owner: TBD          |
| Unexpected rail/back-power behavior or unsafe measurement setup     | Abort per section 3; correct setup before repeating; owner: TBD         |
| Characterization reveals an unanticipated failure path              | Record defect and review affected scope; owner: TBD                     |

- [ ]  Failed cases will not be silently excluded or scope automatically narrowed.
- [ ]  Any narrower scope requires explicit reviewed exclusions and rationale.
- [ ]  Design/setup changes will trigger review and repetition of affected checks.
- [ ]  Final R4 disposition remains a measured G5 requirement, including relevant combined failures.

## 7. G1b review and conditional acceptance

- [ ]  Sections 1–6 identify the actual baseline and all proposed experiments.
- [ ]  Every included experiment has a complete setup, procedure, measurement plan and abort conditions.
- [ ]  Applicable numeric limits are filled in; characterization-only objectives are explicit.
- [ ]  Independent inhibition and instrument grounding/isolation are reviewed.
- [ ]  Proposed R4 disposition, exclusions and rework triggers are recorded.
- [ ]  Requirements/evidence mapping and remaining blockers are recorded.
- [ ]  Time-separated self-review is complete and linked to the reviewed revision.


| Decision field                                    | Entry                                                                  |
| ------------------------------------------------- | ---------------------------------------------------------------------- |
| Disposition                                       | PENDING — choose conditionally accepted / changes required / rejected |
| Approved experiment IDs and exact inhibited scope | TBD                                                                    |
| Excluded/deferred experiments and reasons         | TBD                                                                    |
| Conditions that must be satisfied before running  | TBD                                                                    |
| Review findings and resolutions / evidence links  | TBD                                                                    |
| Reviewer and review timestamp                     | TBD                                                                    |
| Reviewed revision / commit                        | TBD                                                                    |
| Re-review triggers                                | Baseline/setup/procedure changes; additional triggers: TBD             |

Do not mark accepted while an applicable field for the approved scope remains TBD.
After review, update the G1b row in [system status](../system_status.md) with the
actual disposition. Conditional acceptance permits only the recorded B2 scope;
it does not accept G2, close U01/R4, or authorize powered motors.

## 8. Later B2 execution references — not required for G1b plan approval

Add one row per executed case/subcase. Preserve raw captures and actual image/setup
identities. Do not use “no movement” with motor power absent as proof of inhibition.


| Case / run   | Date / operator | Actual baseline and setup | Raw evidence / measured result | Defects / disposition / reviewer |
| ------------ | --------------- | ------------------------- | ------------------------------ | -------------------------------- |
| Not executed | —              | —                        | —                             | —                               |

G2 acceptance is a separate review of the required B2 evidence. Repeat the relevant
failure tests in B5 with the implemented monitor, final candidate circuit and
matched software before final R4 adoption.
