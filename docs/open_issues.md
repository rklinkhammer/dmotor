# Current unresolved system issues

> Architecture update (2026-11-07): main uses the [1 ms PS motor executor](../DMOTOR_ARCH.md) with independent PL capture/PWM/protection and a matched PSF1 driver/daemon. Client contracts, commissioning-first scope, U0 evidence, the hardware pause and pending live recovery are unchanged.

This is the current issue register, extracted from the plan at `20f5972`.
Robert Klinkhammer is the accountable owner for every row; listed subsystem roles
identify the expertise/evidence needed. Time-separated self-review is the selected
acceptance method. No technical issue is closed by this extraction.

| ID / prior finding | Required decision or work | Owner role | Blocks / closure evidence |
|---|---|---|---|
| U01 / R4 | Independent PL-failure response or expressly restricted failure model; rev-A changes if needed | Hardware + PL + system | G1b candidate, final G5/G6; separate clock/reset/config/power fault measurements |
| U02 / R1,R3 and follow-ups | Adopt/amend v4, determine cleanup proof/remaining ABI, implement recovery and ARM admission | PS + ESP32 + PL | G4/G5; transition matrix, shared simulation and physical recovery traces |
| U03 / R5 | Absolute acquisition budget, timeout policy and derived detection limits | ESP32 | G1c/G3; transaction budget and worst-case target traces |
| U04 / R6 | Validate pinned IDF/ISR/flash/WDT isolation; define update inhibition | ESP32 | G3/G5; call/data audit and cache/flash/stall tests |
| U05 / R7 | Bounded Linux diagnostics and scheduler behavior | PS | G4/G5; stdout/storage/load tests and measured deadlines |
| U06 / R8 | Calibration, population, thresholds, persistence, hysteresis and cause-bit allocation | Hardware + ESP32 | G1c/G3/G6; reviewed schema, corrupt/missing sensor tests and calibrated measurements |
| U07 / R2 | Qualify implemented rev-10 pin/PL/UART integration; retain future physical-fault routing as separate scope | Hardware + PL/PS | G2/G5; connector checks and traces (not an assertion that implementation is still absent) |
| U08 / R10 | Normal application ownership, planning, pose/queue policy and vehicle acceptance limits | PS + controls | BV/GV for full-vehicle scope; agreed requirements, simulation and loaded tests |
| U09 | Match newest source/client SDK to ARM/ESP32 release and deployment evidence | Build/release | G7; manifests, target package tests and clean deployment |
| U10 / R9 | Replace accumulated status/history with current documents and archive without losing requirements | Documentation + subsystem reviewers | G1d ownership/matrix; migration completion before G7 with manifest/link audit and no unowned requirements |
| U11 | Owner/review method selected (D02); assign operational duties and approve integrated powered-test procedure | System + hardware/operator | Before respective acceptance tests; B6 entry requires approved procedure and operational controls |

Proposed v4 timing targets (including ISR and stale-sample/LVC bounds) are inputs
to U03/U06 review, not measured facts. Electrical thresholds and vehicle tolerances
remain unspecified where not accepted; this plan does not manufacture values.

## Current dispositions

Physical tests for U01/U03/U04/U06/U07 are paused pending assembled monitor and
motor boards (D04); none is closed. Software/design work can continue. U0 UART
checks passed; U1 build/test evidence exists but physical results are pending.


- **U08:** [architecture discussion](autonomous_motion_architecture.md) active;
  implementation/qualification remains for the subsequent vehicle release. First
  release is commissioning-only.
- **U10:** in progress; five source documents snapshotted with hashes and exact
  originals. Current status/decisions/issues are extracted. Mixed component/architecture
  content and companion-repository inventories still require review and migration.
- **U11:** owner and review-method selection resolved by the user. Integrated powered
  procedure approval, named operators/spotters where required, and completed individual
  gate review records remain open. Self-review is not automatic gate acceptance.
- **U02:** [source audit](g1a_cleanup_admission.md) finds per-motor lazy RESET_CONTEXT
  initialization in the baseline. CLN1 now exposes combined cleanup completion,
  with kernel/PS verification. The previous motor-HLS/AXI cleanup and ARM builds passed; current PSF1 qualification is tracked in [system status](system_status.md). Remaining
  boundary/race coverage, board qualification, review and recovery admission
  integration remain open; build/test success does not close G1a.
- **U01–U07/U09:** remain open as scoped above. Use the [G1 work package](g1_work_package.md)
  for the next concrete contract/proof tasks. Do not close them from historical review tables.

Closed findings and prior review rounds remain in the [archive](archive/index.md);
retain their evidence links when recording a closure here.

G1a/G1c source increment (2026-11-06): [read-only client status](g1a_client_status.md)
and [monitor design tables](g1c_monitor_design.md) are prepared. Neither closes
U02–U06. Review the non-atomic attestation limitation, storage-loss policy, actual
population/thresholds and cycle budget before operational integration.
