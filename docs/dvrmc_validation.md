# DVRMC integration synthesis audit — 2026-10-12

Vivado 2026.1 validated and synthesized an isolated copy of
`/home/rklinkhammer/workspace/dvrmc/dvrmc.xpr`, targeting `xc7z020clg400-1`.
The original project was not modified. The working copy is
`build/dvrmc_validation/project/dvrmc.xpr`.

## Connectivity

The original saved design reproduced BD 41-237 despite the clock converter
being configured for AXI4LITE. Vivado's SMARTCONNECT-2 warning recommends
connecting SmartConnect directly to endpoint IPs. Manual burst-length overrides
on the converter were rejected or overwritten; the previously suggested console
workaround is not reliable in this project/version.

The tested alternative removes the standalone converter, connects
`axi_smc/M04_AXI` directly to `controller_axi_0/S_AXI`, and configures SmartConnect
with two clock inputs: `aclk` at 100 MHz and `aclk1` at 20 MHz. SmartConnect
performs the domain crossing. Validation confirms AXI4LITE and maximum burst
length 1 on both ends. The controller remains at 20 MHz; GP0 remains at 100 MHz.

Apply the same change to the original open block design with:

```tcl
source /home/rklinkhammer/workspace/dmotor/platforms/vivado/fix_dvrmc_connection.tcl
```

This script changes the open design but does not save it automatically. It is a
one-time topology migration and checks the resulting protocol and burst length.
The script was tested in the isolated copy.

Other findings:

- The original converter's `m_axi_aresetn` was unconnected and generated as
  constant zero, holding its master side in reset. Removing that converter
  eliminates this connection. If retained instead, it requires the 20 MHz
  reset generator's `peripheral_aresetn`.
- Controller active-high reset connects correctly to the 20 MHz reset generator's
  `peripheral_reset`.
- Controller mapping is `0x43C00000`, 64 KiB.
- Motor 0/1 encoder inputs and direction/enable/PWM slices map to bits 0/1.
  Encoder inputs for unused motors 2/3 become constant zero. Software must leave
  those channels disabled.
- `halt_async` is driven by AXI GPIO; it is a software-controlled halt input,
  not an independently wired physical emergency-stop input.
- `shared_halt` has no external connection. Internal all-motor inhibit remains,
  but no external system can observe that output through a physical signal.
- `physical_faults_0[3:0]` is exported without pin locations or I/O standards.
  Its physical integration must be decided before bitstream generation.

## Synthesis blockers

Baseline synthesis completed, but DRC failed. The full design used 73,554 LUTs
including LUTRAM/SRL, versus 53,200 on this device. The controller alone used
72,074 LUTs, 50,436 registers, 22 DSPs and two RAMB36 blocks. These are Vivado
mapped synthesis results, not earlier HLS estimates.

The corrected direct-SmartConnect design also synthesized successfully and uses
73,542 LUTs (138.2% of device capacity), 52,441 registers, 22 DSPs and two RAMB36
blocks. The same two loop violations and fault-pin constraint violations remain.

The baseline hierarchical report attributes 32,028 LUTs to flattened controller
top-level logic and 9,533 to the AXI host hierarchy. Synthesis can move/absorb
logic across boundaries: these figures do not mean that the small AXI handshake
module intrinsically requires 9,533 LUTs. Wide configuration/telemetry bank
selection and dynamic indexing deserve an isolated synthesis experiment.

DRC also reports two LUTLP-1 combinational-loop violations through the
scheduler/protocol protection paths. These need RTL root-cause analysis and
regression tests; do not waive the loops. Timing analysis reports additional
loop diagnostics, so post-synthesis slack is not a timing-closure result.

NSTD-1 and UCIO-1 identify the four external fault inputs. Motor signal pins
already have XDC assignments, but that does not qualify their electrical
compatibility with the selected H-bridge.

Next implementation work should remove the protection feedback loops, reduce
the integration MMIO/state-selection logic, and resolve physical fault/HALT
wiring. Then repeat synthesis and perform place-and-route/CDC checks. No
bitstream, programming, or powered-motor test was performed in this audit.

Evidence is under `docs/evidence/dvrmc_validation/`; baseline files have plain
names and the direct-SmartConnect rerun has `_fixed` names. The existing packaged
controller OOC synthesis checkpoint was reused; the SmartConnect and top-level
synthesis were rerun. This audit tests the current exported IP consumed by the
board project, not a fresh HLS regeneration.

Protection-loop follow-up: the [scheduler/arbiter repair](protection_feedback.md)
now passes fresh complete-controller out-of-context LUTLP-1 checks with zero
violations. Updated IP is version 0.18, core revision 2. The original board-audit
reports remain historical evidence; LUT capacity and fault-pin constraints are
still unresolved.

MMIO optimization update (2026-10-12): [fixed bank decoding](mmio_logic.md)
reduces the controller from 72,104 to **34,129 LUTs**, with zero LUTLP-1 violations
and passing expanded AXI/HLS simulation. The controller archive is now core
revision **3** (version 0.18). Earlier resource-overflow reports are historical;
board-level regeneration, implementation and physical fault-pin constraints
remain outstanding.
