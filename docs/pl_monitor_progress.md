# PL monitor integration — revision 10, 2026-11-03

2026-11-04: a full fingerprint-validated EDF build generated and installed a
matched hardware bundle with the confirmed CK_IO2/3 wiring, then built Linux
and exported the SD image. See [current build](current_build_2026_11_04.md).
Earlier development-validation results below retain their historical scope.

Board follow-up: the saved BD now connects HB/coast and all four motors. UART
Lite settings are explicit and the board can be recreated/validated separately
from the user's GUI project. See [Arty interface status](arty_monitor_interface.md)
for the completed pin assignments and the UART/software boundary.

The heartbeat supervisor is now integrated into `controller_axi`, along with
the revision-8 coast output. This is source/simulation and development-IP work,
not an electrically qualified board release. The saved BD includes the new
input and output; the tracked XDC now assigns HB/coast to CK_IO2/3, confirmed on 2026-11-04.

## Controller interface

- **`hb_async` input:** monitor J7-2 (HB_OUT), 3.3 V asynchronous heartbeat.
  The controller contains the two-stage synchronizer and scoped first-stage
  timing exception. Do not connect HB to physical_faults or halt_async.
- **`coast_req` output:** monitor J7-3 (HALT_IN). Asserts for existing local
  protection, synchronized PS HALT/its retained latch, or a monitor HB fault.
- **`halt_async` input:** retains the existing PS AXI GPIO connection.
- `shared_halt`, `physical_faults[3:0]` and `drive_enable[3:0]` remain available.
  The user's general-purpose physical-fault inputs and internal GPIO diagnostic
  use of drive_enable remain supported. Revision 10 registers the diagnostic
  drive_enable output before crossing to GPIO: it trails the raw motor state by
  one 20 MHz clock (50 ns). PWM and coast inhibition are unchanged; this readout
  is not an actuator interlock or an atomic multi-motor snapshot.
  shared_halt retains local-fault attribution;
  a monitor-only fault is reported separately and does not fabricate a motor fault.

HB is mandatory for enabling motors, including direct duty. At the 20 MHz clock,
initial observation allows 1 s for the first edge, followed by a 500 ms healthy
qualification window. After edges begin, a 50 ms missing-edge interval or an
edge interval below the provisional 1 ms minimum latches a fault. Expiration
wins an edge or acknowledgment at the same boundary. A static high/low signal
cannot sustain qualification. These timing constants are not field configurable.

Loss of qualification independently gates outputs and disables the motors;
a latched monitor fault also asserts coast_req. HB resumption alone cannot
restart them. CORE_RESET blocks qualification output but preserves the fault
and continues health observation. An explicit acknowledgment can clear the HB
fault while core reset is held, avoiding a recovery dependency on the HLS core.
Cold fabric reset clears the monitor state; pre-configuration/power/clock-loss
coast remains the open R4 system issue.

## Additive MMIO contract

The DMI2 identity is retained; the following capability was added in revision 9.
The new runtime requires this capability and fails closed on older hardware.
The kernel driver allows these local accesses even when mailbox traffic is blocked.

| Register | Offset | Behavior |
|---|---|---|
| MONITOR_CAPABILITY | 0x1064 | Read-only `0x48423131` (HB11) |
| MONITOR_STATUS | 0x1068 | Read-only: bit 0 healthy observation window; bit 1 sticky fault; bit 2 qualified; bit 3 core reset; remaining bits zero |
| MONITOR_ACK | 0x106C | Write exactly 1; requires disabled motors, core reset, nonzero engine session and healthy observation; otherwise SLVERR |

Accepted ACK is a request, not proof of clearance: read back MONITOR_STATUS.
A simultaneous new timeout wins. Qualified means healthy AND no HB fault AND
not in core reset; it is not monitor ARM/permit confirmation. General STATUS
bit 16 includes monitor faults and bit 18 reports the resulting inhibition.
The existing shared-HALT bit still represents the original shared fault path.

`Runtime::prepare()` now checks capability, waits up to 1.5 s for healthy HB
while disabled/reset, requests acknowledgment and verifies status before
releasing reset. It confirms qualification afterward. Failure retains/asserts
HALT. Neither prepare nor HB recovery enables motors or republishes old targets.
This requires the matching rebuilt driver/runtime; the previous daemon has no
HB recovery support. The new registers do not implement UART recovery or ARM.

## Vivado and deployment

The development export is controller IP **revision 10**, version 0.18, at
`build/current_ip/controller_axi/ip`. Refresh the catalog and upgrade the block.
Connect hb_async to the external input assigned to J7-2; retain coast_req on
J7-3, the PS halt_async source and the saved physical-fault/drive-enable wiring.
Validate/save the BD and copy it back per [build.md](build.md#preserve-gui-edits).
The saved BD is validated in a separate generated project at
`build/monitor_board_check/vivado`; the user's product GUI project is preserved.

Export reused the existing HLS RTL in `build/vitis_atomic` and
`build/product/engine/build/vitis_dispatcher`. The normal fingerprint-validated
`python3 tools/build.py ip` pipeline remains the release build path. New source
lists include the heartbeat module; the packaged constraint cuts only the first
HB synchronizer stage. Board synthesis and routing results are recorded below;
external timing and electrical qualification remain open. No bitstream was
generated or programmed.

## Validation and limits

- Full controller XSim with both real existing HLS kernels passed. Added cases:
  startup refusal, HB loss during direct drive, PWM/enable inhibition, coast,
  status/capability reads, invalid/active/stale acknowledgments, preservation
  across CORE_RESET, healthy resumption with sticky fault and recovery without
  re-enable. Existing velocity, STOP, software HALT, physical faults, watchdog
  and direct-duty tests also passed.
- Native software build with UBSan: 8/8 suites passed. Added explicit-prepare
  tests for missing capability, absent heartbeat timeout and fault recovery
  without enable. Service tests use fake hardware, not a target kernel/PL.
- Generated C++, SystemVerilog and kernel register headers checked against the
  schema. Standalone heartbeat regression from the first increment also covers
  synchronizer-boundary expiry/ACK precedence and too-fast edges.
- Development IP export and its mailbox XSim regression passed; packaged XML
  confirms revision 10, hb_async/coast_req and the preserved controller ports.
  Kernel access module compiled against local Linux 7.0.0-42 headers (not the
  ARM target kernel); BTF generation was skipped because vmlinux was unavailable.

The [PS monitor increment](ps_monitor_progress.md) now supplies framing/schema
and a bounded serial transport. The monitor endpoint, full recovery exchange/physical ARM interlock,
ESP32 firmware, independent PL clock-loss supervision, target EDF build and
physical qualification remain open. A pulse generator can exercise HB on an
inhibited bench but is not a substitute for the sensor-gated monitor firmware
in a powered system. This work does not close all of R1/R2/R3/R4.

## Routed revision-10 follow-up

Vivado 2026.1 completed synthesis and routing in the separate validation project.
[Retained reports and input hashes](evidence/monitor_rev10/inputs.json) describe
this development build; [validation Tcl](evidence/monitor_rev10/validate.tcl)
opens the separately recreated project and runs through route_design.

- [Timing](evidence/monitor_rev10/timing_summary.rpt): WNS +2.570 ns,
  WHS +0.014 ns; zero setup/hold/pulse-width failing endpoints. Controller
  20 MHz setup margin is +11.830 ns. No unconstrained internal endpoints,
  unclocked registers, or combinational loops were reported.
- [Routing](evidence/monitor_rev10/route_status.rpt): all 78,198 routable nets
  routed, zero routing errors. [I/O](evidence/monitor_rev10/io.rpt) confirms
  mon_hb=V15 and coast_req=T15 with LVCMOS33 in that historical run. The
  2026-11-04 CK_IO2/3 selection requires a new matched hardware build.
- [CDC](evidence/monitor_rev10/cdc.rpt): zero critical findings. The first run
  found four CDC-10 findings on combinational drive_enable feeding GPIO.
  Revision 10 fixes these with source registers; no timing waiver was added.
  Three CDC-6 warnings remain: two vendor SmartConnect data crossings and the
  four independent diagnostic bits. GPIO does not provide a coherent snapshot.
  HB and PS HALT are recognized as two-stage ASYNC_REG synchronizers, with
  false paths to their first stages.
- [DRC](evidence/monitor_rev10/drc.rpt): no errors or critical findings;
  40 DSP input/output pipelining recommendations remain.
- [Utilization](evidence/monitor_rev10/utilization.rpt): 36,871 LUTs (69.31%),
  52,459 registers (49.30%), two BRAM tiles, 16 DSPs.
- [Controller XSim](evidence/monitor_rev10/controller_simulation.log) passed
  after the diagnostic change, including an assertion that its output equals
  the preceding sampled raw state. Direct-drive test setup now explicitly
  acknowledges healthy HB under reset, avoiding dependence on HB/reset phase.

This is constrained internal timing closure, not full board sign-off.
[Methodology](evidence/monitor_rev10/methodology.rpt) still reports TIMING-9
(unknown CDC logic) and 40 missing I/O delays (18 inputs, 22 outputs).
The detailed CDC report above has no critical findings, but these methodology
items and external asynchronous-interface timing need explicit review before
release; no blanket exceptions were added. PS DDR board-preset skew warnings
also remain from BD validation. The export reused existing HLS output; a matched
fingerprint-validated release build, hardware bundle and physical tests remain
necessary. Refresh/upgrade the user's GUI controller IP to revision 10 before
building there; its wiring/ports have not changed.
