# Bench diagnostic programs — G1b / B2

**Current scheduling (2026-11-06):** Further physical testing is paused until
assembled monitor and motor boards are available, at Robert's request ([D04](decisions.md)).
Completed U0 results remain valid; U1 is implemented/build-tested, with no physical
U1 acceptance recorded. Procedures below are retained for resumption.

The UART-only diagnostic pair is implemented; [build/run instructions](uart_bench.md)
and evidence distinguish it from the remaining tasks. Their purpose
is to make the [G1b bench checklist](gates/G1b.md) runnable without waiting for the
full monitor/recovery firmware. Keep motor drive power disconnected. One saved
summary and relevant captures per bench session are sufficient.

## Implement first: U0 UART diagnostic pair

| Component | Existing building blocks | Work to implement |
|---|---|---|
| Zynq Linux `dmotor-uart-test` | UART Lite driver/DT in matched EDF export; `software/linux/monitor_uart.cpp`, monitor codec, pseudo-terminal tests | Add `software/apps/uart_test.cpp` with explicit device, run duration/packet count, seed and timeout; report results and exit nonzero on a failed run |
| ESP32-S3 bench application | Board GPIO map in motor-interface-integrated; esp32-dev with ESP-IDF v6.1; DMOTOR firmware directory exists but has no application | Add standalone `software/esp32/diagnostics/bench/` ESP-IDF project, UART test mode and console summary; document build/flash/reset commands |
| Shared diagnostic packet definition | Existing CRC/escaping implementation and golden monitor vectors | Define a test-only envelope with direction, sequence and deterministic payload; distinguish it from operational recovery frames; add golden vectors consumed by both endpoints |
| Build/package instructions | Linux CMake/Yocto and independent ESP-IDF environment | Register Linux tool in source staging/install/image packaging; keep ESP-IDF build separate; record actual image/program version in test summary |

The existing monitor codec validates operational message types and fixed payload
lengths. Do not disguise arbitrary test bytes as RECOVER/ARM messages or assume
that its 64-byte buffer permits every operational message length. Reuse/refactor
framing/CRC primitives where appropriate; a clearly separate diagnostic envelope
must never be accepted as operational recovery. This test pair does not implement
or exercise motor ARM.

Required bench behavior:

- ESP32 drives permit GPIO8/9/10/11 low before starting peripherals/tasks and never
  raises them. GPIO21 HB stays low in the default UART-only mode. No ARM handler,
  persistent recovery state, Wi-Fi/Bluetooth or normal protection claim.
- UART pins: ESP32 TX GPIO17 / RX GPIO18; Arty TX CK_IO0/T14 to monitor J7-5 RX,
  monitor J7-4 TX to Arty CK_IO1/U12; common ground via J7-1/6. Use **115200 8N1**,
  no hardware flow control. Identify Linux UART Lite by its binding/address,
  **0x42C00000**; do not hard-code a guessed `/dev/ttyUL*` number.
- Bounded bidirectional packet generation, with direction/sequence checking and
  payload comparison. Test both individual directions and simultaneous traffic at
  a rate compatible with the UART bandwidth. Include 0x7d/0x7e, zero/0xff, mixed
  patterns, short packets and the diagnostic protocol's maximum payload/frame.
- Report sent/received/verified packets, missing/duplicate/out-of-order packets,
  CRC/framing/payload failures and elapsed time. Echoes alone are insufficient to
  distinguish the two directions; sequence spaces and counters must be separate.
- Stop at the requested count/deadline. Bound buffers, parser work and UART waits;
  handle partial writes and parser resynchronization. Never wait forever for a peer.
- On reset/reconnect, keep permits low and identify the new test run so stale data
  is not counted as success. Do not silently zero error counters halfway through a
  run. Reset/reconnect checks may be separate, explicitly started runs.
- Exclusive UART ownership: keep console/getty and the future monitor daemon off
  the test device. Normal controller operations still go through `dmotor_client`;
  this tool neither opens the controller MMIO device nor enables motors.

Implementation checks: host encode/decode golden vectors; pseudo-terminal traffic,
corruption/escape cases, partial I/O and absent peer; ESP-IDF compile; native Linux
build and ARM/Yocto packaging check. Then run the actual Arty–ESP32 test and save
counts. Host results do not replace that physical test.

## U1 inhibited GPIO / HB modes — implemented, physical checks pending

The same bench app now implements the following modes; see the
[U1 procedure](u1_gpio_bench.md) for USB commands and bench steps:

- Read/report ARM GPIO41, STOP_SENSE GPIO40, HALT_IN GPIO39 and optional nFAULT/ALERT
  levels using the rev-A map. Label readings as raw diagnostic inputs, not calibrated
  protection decisions.
- Add an explicitly selected, bounded synthetic HB mode on GPIO21 for checking the
  wire, PL qualifier and external monitor watchdog. Default is HB off; permits
  remain low in every mode. End of run, timeout or reset stops synthetic HB.
- Print a prominent diagnostic/synthetic-HB mode indication. Do not run it as if
  full sensor-qualified monitor firmware were present; it can keep PL liveness
  satisfied despite absent sensor protection.
- Reuse existing client status/capture for available PL observations. Add only
  missing diagnostic reporting through the established driver/client boundary.

A permit-low diagnostic can observe gating signals, but cannot prove that the
external gate overrides an actively asserted CPU permit. That distinct fixture/
firmware experiment needs its own inhibited setup before adding any permit-test
mode; it is not part of U0/U1 and not a reason to delay UART testing.

## Independent later tasks: B1 / B2 fault injection

| Task | Implementation / completion criterion |
|---|---|
| B1 controller-clock-stop variant | Design a separate diagnostic PL build with an explicit clock-stop control and observable state. Keep AXI/control clock alive, stop the controller clock without asserting its reset, and support recovery. Verify in simulation that a known active output freezes. Do not modify the operational bitstream to add an unreviewed clock kill. Bench-test only without motor drive power. |
| B2a isolated core-reset operation | Existing `dmotor-maint` HALT/prepare can show overall lifecycle behavior, but prepare combines several operations. Inspect existing driver permissions and add a bounded reset-only diagnostic path through the client/service/driver if required; do not use ad hoc raw MMIO alongside the daemon. Verify the emitted reset sequence. |
| B2b/B3 fabric/configuration procedure | Use existing Vivado Hardware Manager/JTAG. Save the exact reset/unconfigure/reload steps and identify which state each operation actually produces. No new application is needed just to reload a bitstream. |
| B4 rail tests | No bespoke application needed for passive rail/level observations. Use the bench app only when observing firmware startup/input reporting. Record which rails and observers remain powered. |

Full acquisition, calibrated thresholds, normal HB gating, physical ARM and recovery
belong to the operational ESP32 application and later G3–G5 checks. Do not wait for
those to implement U0, and do not claim those functions have passed from U0/U1.

## Work order and progress

| Order | Deliverable | Status |
|---|---|---|
| 1 | Shared diagnostic envelope/vectors and Linux UART utility with host tests | Implemented; codec and independent Python PTY tests pass |
| 2 | ESP32-S3 UART-only diagnostic image; native ESP-IDF build | Implemented; pinned ESP-IDF v6.1 build passes |
| 3 | ARM packaging/build plus bench instructions; physical U0 run | ARM package/QA pass; physical U0 PASS 2026-11-06: 1,000-exchange runs, endpoint restarts and expected failure/recovery after disconnection; [G2 record](gates/G2.md) |
| 4 | Inhibited GPIO/HB modes and U1 observations | Implemented; native/IDF validation recorded in [U1 procedure](u1_gpio_bench.md); flashing and physical checks pending |
| 5 | Separate B1 injection variant, isolated B2 reset support and bench procedures | Not implemented; basic Vivado reload tooling already exists |

This work can proceed alongside G1a. Robert chooses which ready cases to run in
[G1b](gates/G1b.md); a missing program blocks only its dependent case. Update this
table with actual build/test results rather than assuming the old SD export has a
new program merely because it was added to the repository.

Current U0 uses bounded request/reply with independent directional payloads.
Simultaneous-stream stress mode and separate per-error-class counters remain
follow-ups; current counters group unexpected run/sequence/payload as mismatched.
See [actual implemented behavior](uart_bench.md) before running.
