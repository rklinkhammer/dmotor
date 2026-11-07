# Interface-preserving LUT optimization experiments

Worktree branch: `dmotor_mipi_opt`; baseline commit: `67cb5da`.
The original dmotor working tree is not changed. Hardware testing remains paused
pending assembled monitor and motor boards. No MIPI IP, board pin change, live
recovery or autonomous-motion release scope is introduced by this branch.

The sweep covers bounded counters, transport word decoding, packed HLS context
storage (BRAM and LUTRAM), direct context access, and area-oriented synthesis.
It is not a proof of globally minimal area. Precision reduction, reduced telemetry,
weaker checking, changed deadlines, and removal of protection are out of scope.

## Invariants

- All module ports, MMIO addresses/values, HLS memory ports and reply layouts.
- Numeric precision, accepted ranges, snapshot and configuration atomicity.
- Reset behavior and per-motor isolation, including invalid selectors.
- STOP/disable/rearm, timeout priority, watchdog and halt behavior.
- Synchronous HLS memory latency, same-address dual-write priority and backpressure.
- PWM phase, reversal dead time, saturation and full signed-input behavior.

## Measurements

`measure.tcl` synthesizes the complete controller against explicitly selected
atomic and motor HLS RTL. Use the same tool version, part, directive and generated
cores for RTL-only comparisons. It saves a DCP, utilization, hierarchy and loop
DRC reports. `implement.tcl` routes a saved DCP with the existing controller clock
and synchronizer exceptions. OOC routing does not qualify physical board IO.

```sh
/opt/vitis/2026.1/Vivado/bin/vivado -mode batch -source experiments/lut_opt/measure.tcl \
  -tclargs RTL_DIR ATOMIC_HLS_DIR ENGINE_HLS_DIR OUTPUT_DIR Default 0
/opt/vitis/2026.1/Vivado/bin/vivado -mode batch -source experiments/lut_opt/implement.tcl \
  -tclargs OUTPUT_DIR/synthesized.dcp ROUTED_OUTPUT_DIR
python3 experiments/lut_opt/summarize.py
```

For a synthesis-directive comparison replace `Default` with `AreaOptimized_high`.
Do not sum HLS estimates with implemented utilization or compare old historical
product reports to these matched OOC results as if they were the same build.

Storage experiments are reproduced through the existing preparation pipeline:

```sh
python3 tools/prepare_shared_mailbox.py --context-storage bram --output build/bram_profile
python3 build/bram_profile/tools/vitis_check.py --kernel dispatcher
```

Use `--context-storage registers` or `lutram` for alternatives. For the legacy
direct-access experiment use `--variant direct --context-storage registers`.
The retained default is registers with unmodified HLS sources. Packed storage
was measured but rejected for the LUT objective. `compact_status.patch` archives
the rejected compact-status experiment; apply it only in a disposable checkout
before preparing a profile. No generated HLS RTL is hand-edited.

## Verification

Run native/RTL CTest, the prepared-profile native suite, HLS C/RTL co-simulation,
and both `controller_axi` and `controller_cleanup` actual-HLS benches. Set
`DMOTOR_ENGINE_PROFILE` explicitly when invoking `platforms/vivado/sim_controller_axi.tcl`.

`equivalence.py` compares PWM and heartbeat outputs cycle by cycle against Git's
baseline for 150,000 cycles with small/non-power-of-two/power-of-two periods,
full signed duty extremes, reset, inhibit and heartbeat faults.
`transport_equivalence.py` uses inert HLS shells and adversarial forced memory
ports to compare every transport read selector, write collisions, timeout,
completion and response backpressure across 5,000 cycles per adapter. `compare_native.py --baseline OLD_PROFILE --candidate NEW_PROFILE` compares
all 16 emitted reply words over 2,449 services against a separately built old
profile. These are
supplementary differential simulations, not formal equivalence proofs.

Both scripts take `--iverilog PATH --vvp PATH [--base IVL_BACKEND_PATH]`.
They obtain baseline modules through read-only `git show 67cb5da:rtl/...`.
Actual-HLS integration benches separately verify that the real kernels operate
through the adapters; inert-shell tests are not presented as kernel validation.

Reports/logs and exploratory build outputs live under ignored `build/opt/`.
Selected evidence and results are recorded in `docs/lut_optimization.md`.
