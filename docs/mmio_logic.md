# MMIO logic reduction — 2026-10-12

The integrated controller now uses 34,129 LUTs, down from 72,104 after the
protection-loop repair. Both measurements are fresh Vivado 2026.1 out-of-context
synthesis of `dmotor_controller_axi` for `xc7z020clg400-1`, using the same generated
HLS kernels. The controller clock remains 20 MHz.

| Resource | Before | After | Reduction |
|---|---:|---:|---:|
| Total LUTs | 72,104 | 34,129 | 37,975 (52.7%) |
| Registers | 50,436 | 50,411 | 25 |
| DSP48 | 22 | 16 | 6 |
| RAMB36 | 2 | 2 | 0 |

The controller now consumes 64.2% of the XC7Z020's 53,200 LUTs, leaving 19,071
before other board logic. This is resource fit at synthesis, not a placed/routed
result or proof of timing closure. Previous complete-board audit results describe
the older package; the board must be regenerated and synthesized with revision 3.

## Implementation

`rtl/controller_axi.sv` formerly performed address-derived variable writes and
reads across packed 2,560-bit configuration banks and a 2,688-bit telemetry bank.
Wide integer offset arithmetic and variable part-select writes synthesized into
large selection/shift circuits.

The replacement uses bounded channel/word fields, fixed-slice writes with shared
address decoders, fixed per-motor configuration commits, and masked-OR word-read
selection. Watchdog refresh writes also use fixed per-channel slices. Full address
range, alignment and strobe checks remain, preventing low-bit aliases.

There is no register-map change, additional MMIO cycle, memory-capacity reduction,
or change to atomic configuration commit and coherent snapshot semantics. HLS
engine code, safety logic, queue behavior and control timing are unchanged.
No BRAM conversion or architectural offload was necessary for this reduction.

Vivado's rebuilt hierarchy attributes much of the eliminated logic to `bus`:
41,566 LUTs before and 3,572 after. Cross-boundary optimization absorbs surrounding
logic into this hierarchy, so it must not be interpreted as the AXI handshake
module's isolated cost.

## Verification and export

The expanded full-controller XSim test passes with both generated HLS kernels.
It tests all 80 configuration words with distinct per-bank values, invisible
staging until commit, rejected incomplete replacement, all 84 telemetry word
selections, all four refresh slots, alignment/strobe/range failures, and the
existing four-motor, STOP, encoder, fault, recovery and watchdog scenarios.

Fresh complete-controller synthesis reports zero LUTLP-1 violations. The package
export's mailbox simulation also passes. Evidence, utilization and loop reports
are in `docs/evidence/mmio_logic/`; baseline evidence remains under
`docs/evidence/protection_loops/`.

`build/current_ip/controller_axi.zip` is version 0.18, **core revision 3**.
Refresh the IP catalog, upgrade `controller_axi_0`, regenerate output products,
and rerun board synthesis. The original `dvrmc` project is unchanged.

Next: validate the upgraded board netlist and routed timing, then resolve the
physical fault-input constraints and electrical adapter integration before motor
bring-up. Retain the protection-loop checks; do not waive board DRC failures.
