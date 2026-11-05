# Current unresolved system issues

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

- **U08:** deferred to the subsequent vehicle release. First release is commissioning-only.
- **U10:** in progress; five source documents snapshotted with hashes and exact
  originals. Current status/decisions/issues are extracted. Mixed component/architecture
  content and companion-repository inventories still require review and migration.
- **U11:** owner and review-method selection resolved by the user. Integrated powered
  procedure approval, named operators/spotters where required, and completed individual
  gate review records remain open. Self-review is not automatic gate acceptance.
- **U02:** [source audit](g1a_cleanup_admission.md) finds per-motor lazy RESET_CONTEXT
  initialization; no combined all-context cleanup-completion proof is exposed.
  Proposed disabled cleanup plus observable completion requires review and implementation.
  Passing adapter-stub reset tests do not close generated-HLS/AXI proof obligations.
- **U01–U07/U09:** remain open as scoped above. Use the [G1 work package](g1_work_package.md)
  for the next concrete contract/proof tasks. Do not close them from historical review tables.

Closed findings and prior review rounds remain in the [archive](archive/index.md);
retain their evidence links when recording a closure here.
