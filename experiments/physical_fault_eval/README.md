# Shared physical-fault latch evaluation

This isolated experiment compares the current four-channel protection cone with a
candidate that moves physical-source sticky bits into one system register. It
**does not modify or replace product RTL**, the Arty BD, MMIO, or FPGA build flow.

Run from the dmotor repository root:

```sh
python3 experiments/physical_fault_eval/evaluate.py
/opt/vitis/2026.1/Vivado/bin/vivado -mode batch \
  -source experiments/physical_fault_eval/measure.tcl \
  -tclargs build/physical_fault_eval \
  -log build/physical_fault_eval/vivado.log \
  -journal build/physical_fault_eval/vivado.jou
```

The generator uses the locally extracted Icarus tools in `build/tools/usr`.
`--seed N --out build/physical_fault_eval_seedN` selects additional deterministic
random stimulus without overwriting the measurement sources.

The baseline uses the unchanged `rtl/safety.sv`. The wrapper reproduces physical
and HALT synchronization, motor disable/acknowledgement gating, global inhibition,
shared HALT aggregation and coast routing from `rtl/motor_io.sv`. Encoder-range,
stop-expiry and engine fault signals are boundary inputs; the complete encoder,
engine, AXI and PWM implementations are outside this measurement. Protective
reset is exercised; `core_reset` is deliberately disconnected in this protection
cone, as in the product's protection state. This does not test full-controller
soft-reset interactions.

The candidate extracts physical bit 2 from each local register into one four-bit
register. It reconstructs existing fault words before timestamp, enable/inhibit
and HALT decisions. Clearing source i retains the existing disabled-bank-i gate
and the rule that any new fault in bank i takes priority over acknowledgement.
The third, intentionally simplified implementation drops the latter rule; the
harness must detect its non-equivalence before accepting the experiment.

Comparisons run after each rising edge and include all fault words, first-fault
timestamps, refresh sequencing/timestamps, enables, external latches, global
inhibit, shared HALT and coast. Directed tests exercise all nonzero physical masks,
source clearing, other-fault overlap, recovery and no automatic restart. Random
stimulus adds watchdog expiry, refresh/session checks, acknowledgement and reset
interleavings. This is differential simulation, not formal or analog CDC proof.

Vivado synthesizes and places each isolated cone on XC7Z020 with the same settings
and a 50 ns clock. It does not route the complete board, model external input/output
delays or qualify timing. Unused core_reset and masked fault-input warnings are
expected; OOC clock-source warnings preclude a board clock-skew claim. Slices are
reported after placement, LUTs and registers after synthesis and placement.

See [evaluation results](../../docs/physical_fault_evaluation.md). No hardware
access, firmware deployment or promotion to the product path is part of this test.
