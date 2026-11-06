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
| Zynq PL | Revision-11 CLN1 cleanup, HB supervision/coast and motor control built and routed; actual atomic/motor HLS lifecycle and interrupted-cleanup tests pass; setup +2.406 ns, hold +0.024 ns, no DRC errors | Electrical response on the assembled system; independent shutdown for PL-only failures |
| Zynq PS | CLN1-aware exclusive driver/runtime, daemon/client/tools and monitor transport built for ARM; driver compilation and five QEMU semantic executables pass | Full monitor recovery/ARM coordinator, persistent endpoint sessions, bounded diagnostic draining, vehicle planner |
| Client API | `dmotor_client`, installed `dmotor::client` CMake target and Yocto SDK packaging; maintenance migrated; 21 native PS tests pass including installed/staged consumer and recovery model | Client/runtime are in the matched ARM build; physical board/client qualification remains |
| ESP32 | Native ESP-IDF v6.1 container pinned by digest in `esp32-dev`; ESP32-S3/C3 environment smoke builds | DMOTOR application, safety core, board drivers and target qualification are not implemented |
| Integration | Shared monitor wire schema and proposed recovery v4; saved wiring map | Joint state-machine tests, measured timing and accepted system release |
| Build evidence | [Current matched hardware/ARM build and SD export](evidence/g1a_product_build/inputs.json); source hashes and export checksums verified | Exported artifacts have not been programmed, booted or electrically qualified |

Builds, host simulations, ARM QEMU and electrical tests are separate evidence
classes. None alone closes a system safety claim. Existing encoder observations
and commissioning work remain useful historical evidence within their recorded
scope, not qualification of the newly integrated monitor system.

## Gate status and immediate work

| Gate/work | State | Next evidence or disposition |
|---|---|---|
| G1a recovery/admission | Open | [Cleanup/admission specification](g1a_cleanup_admission.md): CLN1, actual-HLS integration and matched build pass; remaining race/boundary qualification, admission hold, persistent sessions and A6–A8 recovery semantics remain open |
| G1b electrical/R4 | Open | Complete the [G1b record](gates/G1b.md): inhibited setup, experiment limits and candidate disposition; draft only, no hardware experiment executed |
| G1c monitor design | Open | Acquisition budget, calibration/protection policies and IDF isolation design |
| G1d documentation | [Prepared for self-review](gates/G1d.md) | Current records and initial archive created; matrix has 19 seed rows; extraction/self-review and full inventory migration remain open |
| G2–G6 | Not accepted | No new board/powered evidence produced by this increment |
| BV/GV | Deferred | Not in first-release scope; retain U08 for subsequent vehicle work |
| G7 | Not accepted | Matched release, gate records and completed documentation migration required |

Current EDF artifacts (under `edf-dev/artifacts/`):

- Hardware bundle: `arty-hardware/20261105T032708Z-6227df62/`.
- Matched SD export: `arty-z7-20/20261105T034755Z-c38af1d1/`, including
  `sdcard.wic.xz`, `sdcard.wic.bmap`, manifests and `SHA256SUMS`.
- [Build/test evidence and provenance](evidence/g1a_product_build/inputs.json),
  [hardware manifest](evidence/g1a_product_build/hardware_manifest.json),
  [image build info](evidence/g1a_product_build/build-info.json) and
  [software identity](evidence/g1a_product_build/dmotor-source-version.json).

At export, all 72 software-source hashes and both hardware fingerprints matched
the recorded build checkout. Later source changes are not included in that image. UART Lite is built in; UART Lite console is disabled. I/O confirms HB on
CK_IO2/U13, coast on CK_IO3/V13 and UART TX/RX on T14/U12. The build corrected
prepared-profile test packaging, replaced fixed startup delays with completion
polling and preserved external-HALT attribution when cleanup is canceled. No board
was programmed and no SD card was written. Prior builds remain in the archive.

The [post-pairing G1a model](g1a_recovery_model.md) now implements host-tested
admission/uncertain-outcome semantics, isolated from the daemon. The native PS
suite now has 21 tests, including the isolated [PS session allocator](g1a_endpoint_sessions.md)
and [pairing/replay link](g1a_session_link.md), now connected through an isolated
[recovery wire adapter](g1a_recovery_link.md) with scripted monitor exchange tests and a
[behavioral monitor simulator](g1a_monitor_simulator.md) exercising ARM/fault/expiry
ordering through the real codec. The [Runtime admission boundary](g1a_runtime_admission.md)
now blocks held motion and discards buffered targets. The
[coordinator integration](g1a_recovery_coordinator.md) now tests owner/generation
command admission against the behavioral monitor and Runtime; daemon hookup remains open. The matched image above predates
these model/allocator/link/adapter and Runtime admission increments;
its recorded source hashes remain the authority for what it contains.

The [MMIO attestation path](g1a_cleanup_attestation.md) now supplies coordinator
proof from repeated Runtime register checks, with mid-read fault tests.

The [durable session integration](g1a_session_integration.md) now reserves PS
lifetimes while HALT is confirmed, before PL preparation and HELLO.

Next development: A8 shared client/service recovery-status semantics and
compatibility tests. Target provisioning, ESP32 persistence, storage power-cut qualification,
remaining G1a proof cases and live admission integration remain open. Robert's evidence review and the G1b/G1c decisions remain
prerequisites for system qualification; a successful image build does not accept
those gates.

Physical UART testing belongs to B2/G2 after G1b conditional acceptance, with
motor drive power disconnected and an inhibited ESP32 diagnostic image. It can
precede full recovery firmware; no target UART test is recorded yet. See the
[serial bring-up sequence](system_bringup_plan.md#serial-link-checkpoint).

The first archive pass covers the plan baseline and chronological review/PL/PS/build
records. [Archive manifest and original copies](archive/index.md) preserve provenance.
Other mixed architecture/component documents are still awaiting extraction; this
is not completion of U10. Update this page in place rather than appending progress rounds.
