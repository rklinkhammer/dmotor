# Two-motor shared velocity engine evaluation

> Current product builds: [build guide](build.md). Commands below are specialized
> component/experiment reproduction or historical evidence, not the default build.


This is an isolated 20 MHz experiment based on the velocity-only profile.
The active full-feature source and its reports are unchanged. The selected
experiment is `build/velocity_pair_direct`; the initial copy-based experiment
is `build/velocity_pair`. Neither is a complete motor-controller FPGA top.

## Architecture

`dmotor_dispatcher_pair` adds a one-bit motor selector to the existing service
interface. Two `Dispatcher` contexts hold independent queues, configuration,
velocity-estimator history, motion/ramp/integral state, completion records and
fault state. One invocation updates the selected context. The synthesis report
and generated RTL show exactly one service instance, and DSP use remains 22,
confirming shared computation rather than two independent controllers.

The first variant explicitly copied a selected context into a working object,
called `service`, then copied it back. This created substantial working registers,
selectors and shallow memories. The better variant calls
`contexts[motor ? 1 : 0].service(...)` directly. It accesses state in place and
retains eight BRAM_18K blocks, avoiding the explicit full-context copy.

Only one service may be in flight. Inputs, including the motor selector and
snapshot/configuration, must follow the HLS transaction protocol. The `reset`
argument resets the selected context; physical HLS reset initializes the whole
engine. A reset of one context must not alter the other's state. No new public
MMIO mapping or autonomous two-channel scheduler is implemented here.

## Results

Vitis 2026.1, xc7z020clg400-1, 20 MHz (50 ns clock; 36.50 ns effective budget):

| Implementation | Motors | LUT | FF | DSP | BRAM_18K | Max service clocks | Path ns |
|---|---:|---:|---:|---:|---:|---:|---:|
| One velocity-only dispatcher | 1 | 21532 | 10695 | 22 | 4 | 801 | 34.617 |
| Two independent dispatchers (arithmetic sum) | 2 | 43064 | 21390 | 44 | 8 | 801 each, parallel | 34.617 each |
| Shared engine, explicit context copy | 2 | 32225 | 34912 | 22 | 4 | 845 | 35.206 |
| Shared engine, in-place context access | 2 | 29650 | 24157 | 22 | 8 | 789 | 35.086 |

In-place sharing saves 13,414 LUT (31.15%) and 22 DSP against two independent
velocity-only dispatchers, at a cost of 2,767 additional FF. BRAM count is equal.
Compared with the copy variant it saves 2,575 LUT and 10,755 FF for four more BRAM.
Sharing stateful C++ computation therefore works, but context selection/storage
is not free. Separately synthesized resource estimates are not routed results.

Each service takes at most 39.45 us by synthesis estimate. The maximum initiation
interval is 790 clocks, so two serialized services need approximately 79 us,
before wrapper arbitration/handshake overhead. This leaves substantial room in
the 800 us computation budget. Timing for a complete scheduled system has not
been established by these measurements.

Two shared engines for four motors would sum to 59,300 LUT, exceeding the
53,200-LUT device by 6,100 before AXI, encoder/PWM, protection and integration
logic. This experiment therefore does not establish a four-motor fit. The
pre-route timing estimate passes, but placement/routing is still unverified.
A single four-context engine is a further experiment, not a measured result.

## Verification

Both variants passed Vitis C simulation, synthesis and Verilog RTL co-simulation.
Both also passed the native test under UBSan. The test compares all 40 reply
words for 1,192 interleaved services with two independent C++ Dispatcher objects.
It exercises alternating motor order, different signed setpoints/gains/encoder
histories, peer traffic during descriptor staging, resetting one context while
the other continues, independent completion pressure, queued stop, rejected
opcodes, immediate stop, and fault-triggered authorization removal for both.

The independent objects are testbench references and are not synthesized into
the shared engine. This tests context isolation and functional equivalence;
it does not qualify physical motor behavior or full board integration.

Protection is still external to HLS: encoder capture, PWM, watchdogs and immediate
inhibit must continue independently for each motor. The test models shared HALT
by revoking authorization on both contexts after a fault; this is not a measured
pin-to-pin protective path. A wrapper must latch any motor's fault, inhibit all
motors without waiting for HLS service, suppress late results, give ticks bounded
priority over bus traffic, capture coherent tick snapshots and define when the
staged PWM outputs take effect. AXI response bounds need separate arbitration
analysis; the old single-peripheral tests do not prove the new shared interface.

## Reproduce

From the main repository, with a fresh output path:

```sh
python3 tools/prepare_velocity_pair.py --output build/pair_new
python3 build/pair_new/tools/vitis_check.py --kernel dispatcher
g++ -std=c++14 -O2 -fsanitize=undefined -fno-sanitize-recover=all -Ibuild/pair_new/include -Ibuild/pair_new/tests build/pair_new/tests/pair_test.cpp build/pair_new/src/*.cpp -o build/pair_new/build/pair_native
build/pair_new/build/pair_native
```

Use `--variant copy` for the initial variant. The helper first prepares the
velocity-only profile, then applies the pair patch. It requires `patch` and
refuses to overwrite an existing output. The default direct variant was checked
to reproduce all four modified/new files exactly in a second isolated directory.
Reports, native logs and patches are in `docs/evidence/velocity_pair/`.
The HLS top/report name is `dmotor_dispatcher_pair`; the config/work directory
continues to use the dispatcher flow name for the existing Vitis runner.
