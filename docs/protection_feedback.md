# Protection feedback repair

> Current product builds: [build guide](build.md). Commands below are specialized
> component/experiment reproduction or historical evidence, not the default build.


The 2026-10-12 Vivado board-project audit exposed combinational feedback in the
integrated controller. This repair preserves the 20 MHz clock, MMIO contract,
and immediate PWM inhibition. Control computations remain in C++ HLS.

Three RTL dependencies were corrected:

1. Admission and retirement requests no longer depend on fault decoding from
   their own qualified responses. Reset, external inhibit, target inhibit,
   previously latched faults, deadline, overrun, ownership changes and cancellation
   still block these requests. A rejected response immediately inhibits outputs
   and latches a fault at the next edge; it cannot publish a frame.
2. Engine result validation uses the registered `issued` flag. The HLS adapter
   completes after an accepted start, never as a zero-cycle combinational response.
   This removes the `live_fault -> engine_reset -> engine_ready -> live_fault`
   path. A start accepted on a cancellation edge is still drained, preserving
   the existing peer-context cancellation behavior.
3. The mailbox arbiter's cancellation notification excludes the *combinational*
   inhibit-edge event. An inhibit still immediately blocks transactions and
   response success, poisons an active transaction, and enters the registered
   cleanup mask at the next edge. Explicit invalidation and ownership changes
   still cancel immediately. This removes the indirect
   `target_apply -> target_inhibit -> cleanup -> cancellation -> target_apply`
   loop without delaying the protective output gate.

Regression coverage now models request-qualified admission/retirement responses
and reset-qualified engine readiness. It tests malformed admission and rejected
retirement while asserting immediate inhibition. The mailbox integration test
also injects an invalid future target timestamp during age validation and checks
that all outputs are inhibited, the fault latches, and no frame is committed.
The strengthened scheduler test stalls with the old scheduler and passes with
the repair.

Reproduce the complete-controller synthesis and loop DRC with:

```sh
/opt/vitis/2026.1/Vivado/bin/vivado -mode batch \
    -source platforms/vivado/check_controller_loops.tcl
```

This uses both existing generated HLS kernels and freshly synthesizes the current
integration RTL. It fails if `LUTLP-1` violations remain. This is an out-of-context
structural check, not board placement/routing or timing closure.

The refreshed `build/current_ip/controller_axi.zip` retains version 0.18 and
increments the IP core revision to 2, as does `mailbox_control`. Refresh the IP
catalog and upgrade the affected block in the board project, then regenerate
output products. The original `dvrmc` project has not been changed by this repair.

## Verified results

Fresh complete-controller out-of-context synthesis reports **zero LUTLP-1
violations**. The initial board audit had two violations; an intermediate repair
still had one, which led to the target-age/cleanup correction above. No loop
waivers were added.

The final controller uses 72,104 LUTs, 50,436 registers, 22 DSPs and two RAMB36
blocks. It still exceeds the XC7Z020's 53,200 LUT capacity. This change fixes
protection structure; it is not a resource reduction or timing-closure result.
Vivado notes that out-of-context DRC does not cover every board connectivity
check. Board-level synthesis/implementation must be repeated after IP upgrade.

Five targeted RTL regressions passed; the two affected scheduler/mailbox tests
were rerun after the final arbiter change and passed. Full AXI/controller XSim
with both generated HLS kernels and the export-time mailbox XSim also passed.
Evidence is in `docs/evidence/protection_loops/`.
