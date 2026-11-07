# Current system status

Implementation tracking started 2026-11-04 from dmotor `20f5972`.
[Execution plan](system_bringup_plan.md) · [active decisions](decisions.md) ·
[open issues](open_issues.md) · [G1 work package](g1_work_package.md).

**First release: commissioning-only.** Robert Klinkhammer owns gate acceptance,
using time-separated self-review against recorded evidence. Vehicle work follows;
Vehicle architecture/U08 is now under discussion; BV implementation and GV
acceptance remain deferred, not completed. No gate acceptance or powered approval
is inferred from starting the plan.

## Current work direction — 2026-11-06

Further physical testing is **paused at Robert's request until the assembled
monitor and motor boards are available**. U0 UART exchange/restart/reconnection
checks passed. U1 GPIO/HB modes are implemented, with 24 native tests and an
ESP32-S3 build passing; U1 flashing and physical measurements are not recorded.
The next focus is [autonomous-motion architecture](autonomous_motion_architecture.md),
not another bench test. Commissioning remains the first release; no hardware gate
is closed or waived by this pause.

## Implementation and evidence

| Area | Current state | What is not established |
|---|---|---|
| Zynq PL | Revision-13 PSF1 1 ms controller promoted; atomic HLS, independent protection, coherent PWM and CLN1 retained. Native/PS-C++ AXI tests pass; matched full-board rebuild in progress | Electrical response on the assembled system; independent shutdown for PL-only failures |
| Zynq PS | Four-context C++ motor worker integrated in daemon; PSF1 driver extension. Native tests pass; fresh ARM compilation and eight QEMU executable checks pending | Full monitor recovery/ARM coordinator, persistent endpoint sessions, bounded diagnostic draining, vehicle planner |
| Client API | `dmotor_client`, installed `dmotor::client` CMake target and Yocto SDK packaging; maintenance migrated; 27 native tests pass including installed/staged consumer, recovery model and three PS executor tests | Client/runtime are in the matched ARM build; physical board/client qualification remains |
| ESP32 | Native ESP-IDF v6.1 container pinned by digest in `esp32-dev`; ESP32-S3/C3 environment smoke builds | UART/U1 diagnostic builds pass; operational application, safety core and U1 target qualification remain open |
| Integration | Shared monitor wire schema and proposed recovery v4; saved wiring map | Joint state-machine tests, measured timing and accepted system release |
| Build evidence | [Previous revision-11 hardware/ARM build](evidence/g1a_product_build/inputs.json); PSF1 main rebuild pending | User reports boot and UART PASS; exact target hashes and broader electrical qualification remain unverified |

Builds, host simulations, ARM QEMU and electrical tests are separate evidence
classes. None alone closes a system safety claim. Existing encoder observations
and commissioning work remain useful historical evidence within their recorded
scope, not qualification of the newly integrated monitor system.

## Gate status and immediate work

| Gate/work | State | Next evidence or disposition |
|---|---|---|
| G1a recovery/admission | Open | [Cleanup/admission specification](g1a_cleanup_admission.md): CLN1 exists; previous actual-HLS integration and matched build passed; PSF1 main rebuild pending; remaining race/boundary qualification, admission hold, persistent sessions and A6–A8 recovery semantics remain open |
| G1b electrical/R4 | Open | [G1b bench record](gates/G1b.md) references rev-A designs; [diagnostic programs](bench_diagnostic_plan.md) planned, [UART pair built/tested](uart_bench.md); [physical U0 PASS](gates/G2.md): 1,000-exchange runs and endpoint restart/disconnect/reconnect checks pass; other cases remain open |
| G1c monitor design | Prepared for review | [Design tables](g1c_monitor_design.md); thresholds/population, measured budget and isolation qualification remain open |
| G1d documentation | [Prepared for self-review](gates/G1d.md) | Current records and initial archive created; matrix has 19 seed rows; extraction/self-review and full inventory migration remain open |
| G2 | Open; further physical tests paused; U0 UART PASS | [Recorded UART exchange and restart/reconnect tests](gates/G2.md); other electrical/IO checks remain open |
| G3–G6 | Not accepted | Monitor recovery/protection and powered qualification remain open |
| BV/GV | Architecture discussion active; implementation/acceptance deferred | [Autonomy proposal](autonomous_motion_architecture.md); first-release scope unchanged |
| G7 | Not accepted | Matched release, gate records and completed documentation migration required |

## Previous revision-11 artifacts

These artifacts predate PSF1 and remain the recorded UART-era baseline. They do
not contain the new PS executor. A fresh main build is in progress.

Previous EDF artifacts (under `edf-dev/artifacts/`):

- Previous hardware bundle: `arty-hardware/20261105T032708Z-6227df62` (revision 11).
- Previous SD export: **`arty-z7-20/20261106T005035Z-4c9ce75c/`**, including
  `sdcard.wic.xz`, block map, manifests and SHA256SUMS.
- Built DMOTOR source: **`58cc209060d86da2cdc4a789925f3291bffb98fe`**, clean.
- EDF packaging/pin commit: **`88df49f`**.
- [Image rebuild/export evidence](evidence/arty_uart_image/inputs.json): boot and
  Linux image builds pass; checksums verified and ARM `dmotor-uart-test` found in
  the exported root filesystem. No SD card written, board flashed or physical boot tested.

The revision-11 hardware source match passed; unchanged PL was reused. UART Lite
is built in and is not its console. HB is CK_IO2/U13, coast CK_IO3/V13,
UART TX/RX T14/U12. The [earlier hardware/ARM evidence](evidence/g1a_product_build/inputs.json)
retains timing and CLN1 verification. The previous SD export
`20261105T034755Z-c38af1d1` is superseded for deployment by this UART-enabled image.

The [post-pairing G1a model](g1a_recovery_model.md) now implements host-tested
admission/uncertain-outcome semantics, isolated from the daemon. The native PS
suite now has 27 tests, including the isolated [PS session allocator](g1a_endpoint_sessions.md)
and [pairing/replay link](g1a_session_link.md), now connected through an isolated
[recovery wire adapter](g1a_recovery_link.md) with scripted monitor exchange tests and a
[behavioral monitor simulator](g1a_monitor_simulator.md) exercising ARM/fault/expiry
ordering through the real codec. The [Runtime admission boundary](g1a_runtime_admission.md)
now blocks held motion and discards buffered targets. The
[coordinator integration](g1a_recovery_coordinator.md) now tests owner/generation
command admission against the behavioral monitor and Runtime; daemon hookup remains open. The previous image includes its recorded software baseline. Host-only coordinator
models remain test candidates; their source inclusion does not enable live daemon
recovery. Its recorded source hashes remain the authority for image contents.

The [MMIO attestation path](g1a_cleanup_attestation.md) now supplies coordinator
proof from repeated Runtime register checks, with mid-read fault tests.

The [durable session integration](g1a_session_integration.md) now reserves PS
lifetimes while HALT is confirmed, before PL preparation and HELLO.

The [A8 recovery-status API and compatibility tests](g1a_client_status.md) now
pass; the current daemon truthfully reports monitor recovery unavailable.
[G1c acquisition, protection, storage and IDF isolation tables](g1c_monitor_design.md)
are prepared. Live G1a admission is stopped at the documented contract/proof
review; operational ESP32 implementation awaits population, threshold and budget
decisions. Target provisioning, ESP32 persistence, storage power-cut qualification,
remaining G1a proof cases and live admission integration remain open. Robert's evidence review and the G1b/G1c decisions remain
prerequisites for system qualification; a successful image build does not accept
those gates.

Physical UART testing belongs to B2/G2 after G1b conditional acceptance, with
motor drive power disconnected and an inhibited ESP32 diagnostic image. It can
precede full recovery firmware; [U0 target PASS is recorded](gates/G2.md). See the
[serial bring-up sequence](system_bringup_plan.md#serial-link-checkpoint).

The first archive pass covers the plan baseline and chronological review/PL/PS/build
records. [Archive manifest and original copies](archive/index.md) preserve provenance.
Other mixed architecture/component documents are still awaiting extraction; this
is not completion of U10. Update this page in place rather than appending progress rounds.

U0 diagnostic increment: Linux/PTY tests, ESP32-S3 ESP-IDF v6.1 build and
Arty ARM `dmotor-runtime` package/QA pass. The recorded revision-11 SD export includes the ARM diagnostic utility. [Bench instructions and evidence](uart_bench.md); Robert supplied a physical U0 PASS. [U1 GPIO/HB modes](u1_gpio_bench.md) are now implemented and build-tested;
flashing and physical observations remain pending. Simultaneous-stream stress remains open.

User-reported bench progress: both Zynq and ESP32 boot; the native USB console
shows the ESP32 diagnostic banner. The [U0 UART tests passed](gates/G2.md): initial 100 exchanges plus four
1,000-exchange runs, including both endpoint restarts and reconnection. An
interrupted run correctly timed out at 203 sent / 202 verified. Electrical permit/HB measurements remain unreported.
