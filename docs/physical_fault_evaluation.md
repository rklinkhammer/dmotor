# Shared physical-fault latch evaluation — 2026-11-07

**Recommendation: retain the current product RTL.** A shared physical-source latch
can preserve behavior, but this experiment finds no meaningful resource benefit.
The system-source interpretation and input-0 bumper assignment remain appropriate
independently of the internal storage arrangement.

## Measured comparison

Vivado 2026.1, XC7Z020, 50 ns clock, identical synthesis/placement settings:

| Isolated protection cone | Current RTL | Shared physical latch | Difference |
|---|---:|---:|---:|
| Synthesis LUTs | 678 | 681 | +3 |
| Placed LUTs | 677 | 679 | +2 |
| Registers | 935 | 935 | 0 |
| Placed slices | 331 | 330 | -1 |

The one-slice difference is 0.30% of this isolated cone, with more LUTs and no
register saving. It is not evidence of a repeatable full-board saving. These
counts are **not** comparable to the complete-board 10,430-slice figure: this
experiment excludes the PS engine interface, encoder logic, PWM and other board
logic. The saved Arty BD also ties the physical inputs low, enabling additional
constant optimization absent from this external-input experiment.

No full-board route, hardware build or target measurement was performed. OOC
clock-source and unconstrained I/O timing limitations prevent a board timing claim.

## Behavior and compatibility

The candidate moves four physical-source sticky bits from the channel registers
into one system register, then reconstructs the existing telemetry/status words.
The same fault words feed first-fault timestamp, inhibition and HALT decisions.
This retains source identification and the external register/port contracts.

A subtle existing rule prevents a simpler implementation: **any new fault in a
bank takes precedence over local acknowledgement of that bank**, not just the
physical source. Example: bumper source 0 has latched and is now inactive; an
encoder fault in bank 0 is active while local acknowledgement arrives. The current
RTL retains the bumper bit. Clearing that bit solely because the bumper input is
inactive would change telemetry and recovery behavior.

The compatible central latch must therefore retain the bank-i disabled-state gate
and bank-i new-fault priority. First-fault timestamps are also per bank and shared
with other causes. Changing to one independent system acknowledgement/timestamp
would be a separate behavioral/interface decision, not an equivalent refactor.

Differential simulation passed **121,036 cycles across four seeds**, comparing
fault words and timestamps, enables, refresh sequence/time, external latches,
global inhibit, shared HALT and coast. Directed tests cover every nonzero physical
source mask, release/recovery, live-fault priority and no automatic restart. Random
stimulus adds watchdog expiry, stale refresh sessions, concurrent faults and reset.
A deliberately simplified clear rule was rejected in all four runs.

The protection-cone wrapper leaves core_reset disconnected, matching the existing
protection state's reset wiring. It does not validate complete-controller soft-reset
interactions. Results are cycle-sampled differential tests, not formal equivalence,
full-controller co-simulation, electrical interrupt qualification or CDC proof.

## Disposition and reproduction

Keep the current implementation for now. Reconsider centralization if a future
system-fault status interface is explicitly desired; justify it by clearer status
and recovery semantics, not expected slice reduction. UART/I2C migration to hard
PS peripherals remains a separate resource-saving proposal requiring measurement.

[Experiment and reproduction instructions](../experiments/physical_fault_eval/README.md)
use the unchanged product safety module as the baseline. [Evidence and build identity](evidence/physical_fault_eval/inputs.json)
include generated baseline/candidate RTL, testbench, simulation logs and utilization
reports. No product RTL, BD, register layout, driver or firmware was changed.
Physical testing remains paused and no live recovery implementation is implied.
