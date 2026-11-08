# G1b — hobby bench bring-up record

**Status updated 2026-11-08: G1b remains OPEN.** The pre-PCB diagnostic tools
are implemented and exercised: nominal acquisition/UART, delayed and I/O-only
injection, reboot/UART-loss and explicit restoration meet their diagnostic criteria.
[The G2 result matrix](G2.md#pre-pcb-acquisitionuart-results--2026-11-08) links the
operator evidence. This establishes readiness/execution for that subset only.

Integrated monitor/motor-board testing remains paused pending assembled boards.
U1 electrical acceptance, PL-only failure response (U01/R4), supply/front-end
checks and assembled-fixture readiness remain open. Synthetic heartbeat behavior
is unchanged. No whole-gate acceptance or powered-operation approval is inferred.

This is a practical checklist for bringing up the boards with motor drive power
disconnected. Use the existing design files instead of copying their specifications
here. A short note, photograph and saved console/scope capture are enough for each
run; no separate sign-off forms, equipment inventory or per-board serial-number
system is needed. Robert prepares, runs and reviews the work. Do a second read
before starting; record the date and any changes below.

G1b means “this particular bench test is ready to run.” It does not require all
G1a software or full ESP32 firmware. Approve/run a subset when its tools are ready;
leave the other rows pending. G2 records the resulting bring-up checks; R4 (PL-only
failure response) stays open until its behavior is measured and a solution chosen.

## 1. Existing baseline — do not re-document the boards

Prepared/operator/reviewer: **Robert Klinkhammer**. Preparation date: **2026-11-05**.
Scope: commissioning-only first. Source baseline supplied by Robert:
`dmotor 48601b371cce9c59e1fa8ed8de689500155513b0`, plus this documentation update.

| Item | Current recorded version / source | What to note at the bench |
|---|---|---|
| Arty Z7-20 | Robert's board identifier: **DB3FA05**; [Arty wiring](../arty_monitor_interface.md) | Only note if a different board is used; this identifier is not assumed to be a PCB revision |
| Monitor PCB | **rev A**, schematic dated 2026-10-30; [monitor design](../../../motor-interface-integrated/monitor/README.md), [schematic](../../../motor-interface-integrated/monitor/monitor-board.kicad_sch) | Confirm received/assembled board matches the ordered design; note substitutions/rework only |
| ESP32 module | Design specifies **ESP32-S3-DevKitC-1**; Robert's entered label: **ESP32-33** | Confirm actual module marking; retain the entered label rather than treating it as a verified part number |
| Motor PCBs | **rev A**, schematic dated 2026-10-30; [motor schematic](../../../motor-interface-integrated/motor-board/motor-board.kicad_sch) and [PCB](../../../motor-interface-integrated/motor-board/motor-board.kicad_pcb); four channels in system design | Write which channels are plugged in; no need to assign serial numbers |
| Hardware repository | `motor-interface-integrated 6bb5255d4d6d3bc44a89b5c3ca9390ba6a2e07d5`; both schematic title blocks say rev A | Current checkout has a local motor-board `.kicad_pro` modification; do not assume it is a board revision or that a later checkout equals the ordered Gerbers |
| Existing inhibit hardware | NC STOP chain; monitor **MAX706R + 74HC08** watchdog/HALT gate, Q2 and permit pulldowns; [I10 circuit notes](../../../motor-interface-integrated/ISSUES.md#i10--watchdog--halt-permit-gate--added-2026-11-01) | Note actual supervisor substitution if any. This supervises monitor HB; it does not by itself solve PL-only clock-stop R4 |
| Harnesses | [Saved connector/pin mapping](../arty_monitor_interface.md); monitor MON headers map to motor J4; unused MON STOP-chain positions need the documented 9–10 jumper | Continuity check the actual cables; a photo plus deviations from the map is enough |
| Rework | **None**, as entered by Robert | Update only if changes are actually made |
| Built Zynq PL | **Revision 11 / CLN1**, hardware bundle `arty-hardware/20261105T032708Z-6227df62` | Record if a different or diagnostic bitstream is loaded |
| Built Linux/PS image | Matched export `arty-z7-20/20261106T005035Z-4c9ce75c`; UART Lite built in, not its console; [current image evidence](../evidence/arty_uart_image/inputs.json) | Record the image directory/name used; reuse its manifest rather than transcribing hashes |
| Built PS source | Clean `58cc209` includes the UART diagnostic; EDF pin/package commit `88df49f` | Current SD export contains the tool; deploy it before trying the Arty test |
| ESP32 application | UART-only diagnostic built; operational firmware not implemented; [firmware location](../../software/esp32/README.md); `esp32-dev` pins ESP-IDF v6.1 | Robert reports diagnostic firmware running with native USB logging; operational firmware remains pending |

Hardware source is already recorded in `motor-interface-integrated`; this table
links it rather than commissioning a new hardware documentation exercise. Old
README implementation-status prose may lag the actual routed files. Rev A labels
and the source snapshot are verified here; actual assembly and flashed images are
still bench observations.

## 2. Software needed — what exists and what must be written

Detailed implementation tasks and expected results are in the
[bench diagnostic program plan](../bench_diagnostic_plan.md). The U0 pair is now built; [instructions and evidence](../uart_bench.md) describe
what is available. Other tools remain planned unless marked existing.

| Test | Required Zynq/PC software | Required ESP32 software | Readiness |
|---|---|---|---|
| Power/continuity/STOP wiring inspection | None; meter/scope | None; no claim of firmware protection | Can prepare now; decide the actual supply hookup first |
| UART connectivity (U0) | **`dmotor-uart-test` implemented** Linux utility; reuse existing UART transport/codec and UART Lite driver | **`software/esp32/diagnostics/bench/` implemented** ESP-IDF app, UART test mode, permits always low | Physical U0 PASS: 1,000-exchange runs, both endpoint restarts and disconnect/reconnect checks; [G2 record](G2.md) |
| Read HB/HALT/STOP and diagnostic GPIO (U1) | Existing daemon/client status tools for existing readings; extend the bench utility/client tooling only for missing diagnostics | Same bench app: input reporting and separately selected bounded HB test mode; permits stay low | GPIO/HB modes implemented; [U1 procedure](../u1_gpio_bench.md); flash and physical observations pending |
| CORE_RESET / PS HALT observations (B2a) | Existing `dmotor-maint` uses the client library for HALT and prepare; add a separate reset-only diagnostic command/path if isolated reset edges are needed | Bench app for HB/input reporting when needed | Existing prepare is a multi-step sequence, **not** an isolated reset injector |
| Fabric reset / unconfigured / reload (B2b/B3) | Existing Vivado Hardware Manager/JTAG; a short saved procedure for the actual reset/reload operation | No firmware needed to observe static electrical levels; bench app if observing monitor behavior | No new FPGA-reset application required; exact operation and probe setup still to choose |
| Frozen active PL output (B1) | **New diagnostic PL build + control procedure** to stop only the controller clock with a known output; keep AXI/control observable | Bench app for inputs/HB as needed; actual PL-failure protection later needs its implemented detector | Injection design/build missing; a software HALT or CORE_RESET is not this experiment |
| Rail removal/restoration (B4) | No special program for static measurements; console logs optional | None for passive measurements; bench app for reset/startup observations | Pick one rail and a safe disconnection method per run; no software can stand in for rail measurements |
| Actual recovery/ARM and protection response | Completed PS coordinator/daemon integration | Full monitor firmware and implemented protection | Later G3–G5 work; not needed for U0/basic electrical characterization |

The UART test is a dedicated test pair, not a motor-control client. Normal
controller operations continue to use `dmotor_client`; do not create another raw
MMIO access path. Stop any competing monitor-UART owner before the diagnostic.

## 3. Simple pre-run checklist

Complete only for the test being run. Do not invent values for absent equipment.

- [ ] Motor drive power physically disconnected; identify the unplugged connection: **fill in at bench**. Motors can remain unplugged too.
- [ ] Physical STOP connected/held during reset or reconfiguration; unused STOP-chain positions jumpered as designed.
- [ ] Actual logic/monitor supply hookup and current limit chosen: **fill in at bench**. Do not assume USB and board power can be paralleled. J7 carries signals/ground, not power.
- [ ] Grounds/probes connected to the intended ground reference; 3.3 V logic interfaces checked. Note any back-power path when a rail is removed.
- [ ] UART/monitor diagnostic image keeps GPIO8/9/10/11 permits low and has no ARM path. An absent image blocks its test only.
- [ ] Know which supply/USB lead to disconnect if anything heats unexpectedly, draws unexplained current, or a signal exceeds the intended rail. Stop and investigate rather than continue.

Record instrument details only when they matter to interpreting a measurement
(e.g. probe reference, time scale and threshold for a response-time trace). Passive
motor-power disconnection prevents drive during these tests; it does not prove the
protection circuitry works. Inhibit-path measurements must still observe the real
signals and identify any unpowered circuitry.

## 4. Bench sequence and observations

Start with **U0**, once its two programs exist. Keep later experiments independent
so unfinished fault-injection software does not delay a basic UART check.

| Case | Practical sequence / observation | Remaining preparation |
|---|---|---|
| U0: UART | Verify ground and crossed TX/RX; run counted bidirectional packets at **115200 8N1**; save summary; repeat after each endpoint reset and cable reconnect | Initial and 1,000-exchange runs passed on `/dev/ttyUL0`; restart/disconnect/reconnect checks complete |
| U1: GPIO/HB | With permits low, report HALT/STOP/ARM inputs; observe explicitly selected diagnostic HB on GPIO21 and Arty qualification/status | [U1 modes and procedure](../u1_gpio_bench.md) ready for bench deployment; no sensor calibration needed for synthetic signal |
| B1: frozen clock | Establish known active PWM/PH logic with motor power absent; stop the controller clock; scope PWM/PH, coast, permits and DRVOFF; restore the clock/reset | Design the diagnostic clock-stop mechanism and document how to restore it; characterize behavior, do not assume shutdown |
| B2a: core reset | Observe core reset separately from PS HALT and ordinary prepare; scope output/coast behavior and monitor input | Add isolated reset mode if needed; label which source was exercised |
| B2b/B3: fabric/configuration | Hold physical STOP; record levels before, during and after the chosen JTAG reset/unconfigure/reload operation | Save exact Vivado operation and image name; don't infer unconfigured behavior from CORE_RESET |
| B4: rail loss | Remove/restore one identified rail at a time; observe rail, signal and back-power behavior; retain measurement reference | Note which devices remain powered. A low voltage on an unpowered device does not prove inhibition |

For each scope experiment record initial output state, what was changed, and
observed output levels/timing. Attach a capture if the result depends on timing.
Use a few repeat runs to confirm reproducibility; increase repetition only when
behavior is inconsistent. No invented pass time is needed for initial
characterization. Before claiming a protection bound, choose and test that bound.

## 5. R4 decision after characterization

No R4 solution has been selected by this record. Use the measurements to decide
whether independent PL-progress supervision, rev-A rework, or a clearly narrower
supported operating scope is needed. Existing ESP32 watchdog/HALT gating does not
detect every frozen-PL condition. Do not treat a failing case as silently excluded.
If a change is needed, note it and repeat the affected checks. Actual protection
with the final firmware/circuit is revisited in G5.

## 6. Robert's readiness note and run log

A dated note is enough; no separate reviewer or signature ceremony is required.
Keep the previously agreed second look at the plan before running a new setup.

- Executed case: **U0 UART — PASS**, supplied by Robert; [G2 evidence](G2.md). No retrospective pre-run checklist completion is inferred.
- Pending cases / reason: **U1 flash/observations pending; B1 injection missing; other cases need the bench hookup/procedure noted**.
- Second-look date and changes: **fill in**.
- Bench supply hookup, populated motor channels and deviations from drawings: **fill in**.

| Date / case | Image or program used (or existing manifest link) | What I did / observed | Capture or log, issue / next action |
|---|---|---|---|
| 2026-11-06 / U0 | `dmotor-uart-test`, diagnostic v1; `/dev/ttyUL0`, 115200 8N1 | Corrected a wire; 100 sent / 100 verified, rejected=0, mismatched=0, 1395 ms | [Console capture](../evidence/uart_bench_target/2026-11-06-u0.txt); [G2 record](G2.md); follow-up checks recorded below |
| 2026-11-06 / U0 follow-up | Same diagnostic v1, `/dev/ttyUL0`, 115200 8N1; exact deployed hashes not supplied | Four 1,000-exchange PASS runs (longer run, ESP32 restart, Zynq restart, reconnect); interrupted run timed out at 203 sent / 202 verified as expected | [Supplied results](../evidence/uart_bench_target/2026-11-06-u0-followup.txt); [G2 details](G2.md) |

Update [system status](../system_status.md) when a case is ready or completed.
A useful result is “these packets passed” or “this pin remained high for this
interval,” not simply “no movement.” G1b readiness is separate from G2 evidence,
and neither alone authorizes powered motor tests.

## Monitor follow-up implementation — 2026-11-08

[Implementation and validation record](../monitor_next_steps.md) adds explicit receiver
rejections, physical-device freshness/configuration checks, calibration integrity,
16 ms recovery simulation and a serialized safety-core candidate with a
hard-inhibited task adapter. U03–U06 and G1a/G1c/G3–G5 remain open: no calibrated
operational application, sustained conversion-age proof, ISR/watchdog qualification
or new electrical hardware result is asserted. The one-INA six-read path and
four-INA 24-read path require new timing evidence. G1b/G2 are not closed by builds.

## Consolidated physical-health disposition — 2026-11-08

[Current G2 matrix](../gates/G2.md#physical-ina-health-results--2026-11-08) records
one-INA nominal PASS (one warm-up + 99 good; max 3,918 us) and wrong-address
rejection PASS (100 io failures; max 739 us). The updated receiver reason test now passes with explicit `failure` and 400
injected failures without skips/expirations. Expected-register mismatch is
operator-reported completed elsewhere; raw-evidence linkage remains pending. These diagnostic subcases advance
U03/U06 evidence, not SYS-03–06 or electrical acceptance. G1a/G1b/G1c/G2 remain
open; G3–G6 and G7 remain unaccepted. Commissioning-first scope, 1 ms PS loop,
16 ms monitor/25% reserve and assembled-board pause are unchanged.
[Next implementation and validation](../system_status.md#next-implementation-and-validation).
