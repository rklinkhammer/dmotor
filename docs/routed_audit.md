# Routed DVRMC audit — 2026-10-12

Examined the fully routed `dvrmc.runs/impl_1` reports dated 23:26 and opened
`design_1_wrapper_routed.dcp` read-only to generate detailed CDC and clock-path
reports. No source, constraint, board-design or routed-checkpoint changes were made.

## Timing passes the specified constraints

| Domain/check | Worst slack |
|---|---:|
| 100 MHz setup | +2.976 ns |
| 20 MHz setup | +13.360 ns |
| Overall hold | +0.007 ns |
| Pulse width | +4.020 ns |

TNS and THS are zero, with no failing setup/hold endpoints. All five reported
SmartConnect bus-skew constraints pass (minimum slack +39.280 ns). The routed
report finds zero combinational loops and no unconstrained internal endpoints.
The small positive hold margin is a passing result, not a reason to reduce the
clock frequency. Missing external timing intent and CDC findings still prevent
calling the interface timing fully qualified.

## Warnings that require attention

**TIMING-6 and TIMING-7: clock relationship.** `clk_fpga_0` (100 MHz) and
`clk_fpga_1` (20 MHz) are separate primary clocks, but some crossing paths are
still timed as related. The direct custom crossing is AXI GPIO 3's output to
controller HALT synchronizers. A 5:1 frequency ratio does not establish the
phase relationship needed for synchronous timing analysis.

**Four CDC-11 critical findings: HALT fan-out.** Detailed CDC identifies the
same `axi_gpio_3/.../gpio_Data_Out_reg[0]` launching into each of the four
`controller/channel[n].safety/halt_meta_reg` chains. Each has two stages and
ASYNC_REG, but their independent sampling can disagree for a clock cycle.
Recommended correction: synchronize external HALT once at the shared motor-I/O
boundary, distribute that synchronized result in the 20 MHz domain, and constrain
the asynchronous path only to the first synchronizer stage. Preserve the
protective latch/recovery behavior and verify the digital inhibit latency.

**TIMING-9 and two CDC-6 warnings: SmartConnect data CDC.** The detailed CDC
report identifies read/write data vectors inside SmartConnect's asynchronous
conversion. These are bundled data associated with vendor handshake logic, not
arbitrary unsynchronized MMIO wires. All five bus-skew checks pass. Review the
vendor constraints/handshake structure before applying any scoped waiver.
Do not add blanket asynchronous clock groups merely to remove warnings: they
can override the existing datapath-only max-delay constraints on these crossings.

**94 TIMING-18 warnings: external I/O timing.** The report lists 46 inputs and
48 outputs without timing delays. These include buttons, switches, shield GPIO,
I2C, motor encoders/faults and motor outputs. They need classification by actual
interface, not invented input/output delays:

- Asynchronous encoder/fault inputs: constrain the first synchronizer stage;
  keep the stage-to-stage path timed. `qcencoder` already marks its synchronizers
  ASYNC_REG; `motor_io` physical fault synchronizers lack that attribute.
- Synchronous external interfaces: specify their actual external timing contract.
- Static/asynchronous outputs and control pins: document and constrain their
  intended electrical/timing behavior separately, including driver requirements.

## Routed DRC report

There are **no Error or Critical Warning entries** in the latest standard routed
DRC report. Its 40 warnings are DSP pipelining advice:

| Rule | Count | Meaning |
|---|---:|---|
| DPIP-1 | 8 | DSP input pipelines unused |
| DPOP-1 | 16 | DSP output pipeline unused |
| DPOP-2 | 16 | DSP multiplier pipeline unused |

These are optimization suggestions, not current setup failures. The 20 MHz
controller has +13.360 ns setup slack. Changing HLS multiplier latency solely to
silence them would require rechecking HLS scheduling, deadlines and co-simulation.
The prior LUT overflow, combinational-loop, missing pin location and I/O-standard
violations are absent from this routed DRC report.

Recommended order: consolidate/constrain HALT CDC; mark/constrain physical input
synchronizers and classify remaining external I/O; rerun CDC/methodology and
routing. Address DSP pipelining only if later performance/power evidence warrants
it. Original reports remain in the dvrmc implementation directory; newly generated
read-only diagnostics are saved under `docs/evidence/routed_audit/`.

Follow-up: controller revision 4 implements shared HALT synchronization and a
scoped first-stage timing exception. Local synthesis and tests pass; the original
revision 3 routed findings above remain historical evidence until the board is
upgraded and rerun. See [architecture status](architecture_status.md).

## Rebuilt revision 4 evidence — 2026-10-13

The new routed checkpoint confirms the HALT repair: TIMING-6/TIMING-7 and all
four CDC-11 violations are gone. HALT is one CDC-3 informational crossing with
two ASYNC_REG stages and its first-stage false path. Setup slack is +2.713 ns
(+12.072 ns in the controller domain), hold +0.025 ns, with no failing endpoints.
Two CDC-6 SmartConnect bundled-data warnings and 94 external-I/O timing warnings
remain; standard DRC contains only the 40 DSP pipeline warnings.

The implementation log also exposed unsupported `if`/`unset` commands in the
packaged HALT XDC. The false-path constraint itself applied, but the diagnostic
Tcl commands produced critical parser warnings. Revision **5** removes those
commands, retaining only the supported constraint. The corrected XDC parsed
successfully against the routed checkpoint; the archive was re-exported and its
mailbox simulation passed. There is no RTL change from revision 4. Original board
files/checkpoint were not modified. Upgrade the IP and regenerate output products
to consume this packaging correction. Evidence: `docs/evidence/routed_rev4/`.
