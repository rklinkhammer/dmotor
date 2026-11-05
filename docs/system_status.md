# Current system status

Implementation tracking started 2026-11-04 from dmotor `20f5972`.
[Execution plan](system_bringup_plan.md) · [active decisions](decisions.md) ·
[open issues](open_issues.md) · [G1 work package](g1_work_package.md).

**First release: commissioning-only.** Robert Klinkhammer owns gate acceptance,
using time-separated self-review against recorded evidence. Vehicle work follows;
BV/GV and U08 are deferred, not completed. No gate acceptance or powered approval
is inferred from starting the plan.

## Implementation and evidence

| Area | Current state | What is not established |
|---|---|---|
| Zynq PL | Revision-10 motor control, HB qualifier/inhibit/latch/ACK, global coast output, registered drive-enable diagnostics; routed build evidence | Electrical response on the assembled system; independent shutdown for PL-only failures |
| Zynq PS | Exclusive driver, runtime, daemon, typed client, maintenance tools, monitor codec/UART transport; native tests and tooling | Full monitor recovery/ARM coordinator, persistent endpoint sessions, bounded diagnostic draining, vehicle planner |
| Client API | `dmotor_client`, installed `dmotor::client` CMake target and Yocto SDK packaging; maintenance migrated; 14 native tests pass including installed/staged consumer | Latest ARM package/image and board validation must be tied to their actual source manifest |
| ESP32 | Native ESP-IDF v6.1 container pinned by digest in `esp32-dev`; ESP32-S3/C3 environment smoke builds | DMOTOR application, safety core, board drivers and target qualification are not implemented |
| Integration | Shared monitor wire schema and proposed recovery v4; saved wiring map | Joint state-machine tests, measured timing and accepted system release |
| Build evidence | Recorded 2026-11-04 matched PL/EDF/ARM build and SD export | That dated record does not prove subsequent client/tooling changes are in a deployed image |

Builds, host simulations, ARM QEMU and electrical tests are separate evidence
classes. None alone closes a system safety claim. Existing encoder observations
and commissioning work remain useful historical evidence within their recorded
scope, not qualification of the newly integrated monitor system.

## Gate status and immediate work

| Gate/work | State | Next evidence or disposition |
|---|---|---|
| G1a recovery/admission | Open | Resolve the proof/transport/persistence decisions in the G1 work package before adopting v4 |
| G1b electrical/R4 | Open | Complete inhibited experiment setup, limits and candidate disposition; no hardware experiment executed |
| G1c monitor design | Open | Acquisition budget, calibration/protection policies and IDF isolation design |
| G1d documentation | [Prepared for self-review](gates/G1d.md) | Current records and initial archive created; matrix has 19 seed rows; extraction/self-review and full inventory migration remain open |
| G2–G6 | Not accepted | No new board/powered evidence produced by this increment |
| BV/GV | Deferred | Not in first-release scope; retain U08 for subsequent vehicle work |
| G7 | Not accepted | Matched release, gate records and completed documentation migration required |

Current native client/tooling evidence is distinct from the older full EDF image.
The [dated full-build record](archive/pre-system-plan-2026-11-04/docs/current_build_2026_11_04.md)
records hardware bundle `20261104T231424Z-88b1f40c` and SD export
`20261105T000706Z-ce4bbc9b` in edf-dev. Its source manifest, not its date or title,
defines what is installed. No new target image or board qualification is claimed.
[Retained manifests and reports](evidence/current_build_2026_11_04/) remain in place.

The first archive pass covers the plan baseline and chronological review/PL/PS/build
records. [Archive manifest and original copies](archive/index.md) preserve provenance.
Other mixed architecture/component documents are still awaiting extraction; this
is not completion of U10. Update this page in place rather than appending progress rounds.
