# Verification record — 2026-10-11

Architecture: 0.12. Implementation: AXI/HLS command path plus independent control components, not release
ABI 1.0. The full controller is not integrated. Evidence is deliberately scoped.

## Executed checks

Native/RTL regression: **13/13 passed**, with GCC 15.2.0, UBSan, Python 3.14.4,
and Icarus Verilog 12.0. RTL tests use a 10 ns clock.

| Test | Evidence |
|---|---|
| Protocol reference | All six opcode shapes, signed decoding, relative overflow, 32-bit counter wrap, slot locking, immutable active copy, full completion backpressure, cancellation preservation, stale sessions, and rejection completion |
| Generated ABI | Checked-in C++/RTL/documentation match JSON draft |
| Safety RTL | 36 fault/source combinations across four controllers, source attribution, live-source clear refusal, explicit recovery, stale-session/wrong-sequence refresh refusal, same-cycle expiration priority, and one watchdog stopping all motors while the others are refreshed |
| Scheduler RTL | Held start/ready, valid completion, measured latency, deadline-boundary rejection, inhibited drain; accelerated 20/12-cycle tick/deadline parameters |
| QCEncoder RTL | X1/X2/X4, both directions, invalid transitions and position saturation at 1,000-clock state dwell |
| PWM RTL | 50% duty, full-frame reversal blanking, direction held during inhibit, inhibit bypassing PWM boundary; accelerated 20-cycle period |
| HALT RTL latency | 10 external input phases, 1 ns spacing against 10 ns clock, each within the proposed 50 ns digital bound; no analog metastability claim |
| PI native | Signed rounding, anti-windup, saturation and invalid-range handling |

Icarus was downloaded as the Ubuntu iverilog package and extracted under
`/tmp/dmotor-tools/root`, without system installation. The build cache records
its explicit paths; for a fresh machine install Icarus or pass equivalent CMake
paths as documented in README.

## Vitis evidence

Vitis 2026.1 under `/opt/vitis/2026.1/Vitis`, target `xc7z020clg400-1`,
100 MHz. C simulation and HLS synthesis passed for the **PI kernel only**.
The [retained HLS report](evidence/vitis_pi_csynth.rpt) estimates:

- Latency: 18 cycles (180 ns), initiation interval 19 cycles.
- Estimated combinational timing: 7.014 ns with 10 ns requested clock and
  2.70 ns uncertainty. This is not post-route timing closure.
- Resources: 56 DSP, 3,370 LUT, 3,420 FF, no BRAM.

Four PI instances would need 224 DSPs versus 220 available on this part, before
other logic. Width/operator/resource optimization is required for that deployment;
no four-motor fit claim is made. The large constant-division implementation should
be reviewed before integrating the final estimator/profile arithmetic.

Verilog RTL co-simulation through XSIM passed all five PI transactions, with 18-cycle latency and 19-cycle interval. See [the retained co-simulation report](evidence/vitis_pi_cosim.rpt). The [native/RTL test transcript](evidence/native_rtl_tests.log) records the 13 CTest results. The first legacy Tcl attempt failed to include the design source in C simulation; it was replaced by the successful configuration-file flow in tools/vitis_check.py.

## Architecture coverage and remaining work

V01/V02/V05/V06 have partial component RTL evidence; V03 has native and HLS RTL co-simulation evidence for the command engine,
with bus-level coverage now added for the ring subset. V04 (controlled-stop executor) and V07 (estimator and plant tracking) remain pending.
V08 now covers the ring/register subset over AXI, not the complete register map.
V09 has Vitis PI, command engine and peripheral evidence:
SmartHLS was not found in PATH or the inspected /opt installation. V10 is not run.

No complete HLS/scheduler/PWM connection, bus reset/drain adapter, external
clock-loss protection, current-limit hardware, or physical vehicle has been tested.
Nominal timer calculations in the architecture are consistent, but accelerated
scheduler/PWM tests do not establish the full integrated timing budget.

Before claiming autonomous infrastructure complete, implement and test the full
AXI/ring/state/configuration path and both vendor adapters. Before motor operation,
complete the selected electrical adapter and board-specific validation. Keep
pending calibration values configurable and measured rather than inventing
application acceptance tolerances.


## C++ HLS command engine — revision 0.10

`python3 tools/vitis_check.py --kernel command` passed C simulation, synthesis,
and Verilog RTL co-simulation on `xc7z020clg400-1` at 100 MHz. Co-simulation
covered **386 internal transactions**, including full completion backpressure,
publication locking, cancellation over completion, malformed descriptors, stale
session/completion rejection, and quiesce/reopen ordering. The native regression
also covers the ring's modulo counter wrap and relative-position overflow.

HLS estimates: **10–272 cycles** per transaction (0.10–2.72 us), 11–273-cycle
initiation interval, **11,088 LUT, 2,514 FF, 2 BRAM_18K, 0 DSP**. Estimated clock
path is 7.074 ns against a 10 ns target with 2.70 ns uncertainty. These are synthesis
estimates, not routed timing/resource results. Reset/reopening includes memory
initialization and contributes to the latency envelope.

See [synthesis](evidence/vitis_command_csynth.rpt) and
[co-simulation](evidence/vitis_command_cosim.rpt). The engine uses common portable
C++ rather than handwritten ring RTL. Its LUT footprint and the existing PI DSP
footprint still require integrated multi-instance fit assessment.

The revision 0.10 experiment showed that an eight-clock MMIO bound could not be
met by directly exposing the engine. Revision 0.11 adopts the proposed 2,048-clock
HLS-first budget and adds actual AXI/generated-RTL measurements described below.
Independent protection remains necessary because cancellation accounting can wait
for a running HLS transaction. The command-engine-only test does not cover mid-transaction protection; the
revision 0.11 integrated adapter test adds that case.


## AXI/HLS peripheral — revision 0.11

Vitis 2026.1 C simulation, synthesis, and Verilog RTL co-simulation passed for
`dmotor_peripheral` at 100 MHz on xc7z020clg400-1. HLS estimates **35–304 cycles**,
**12,814 LUT, 3,212 FF, 2 BRAM_18K, zero DSP**, and 7.143 ns estimated clock path
(10 ns target, 2.70 ns uncertainty). See [synthesis](evidence/vitis_peripheral_csynth.rpt)
and [co-simulation](evidence/vitis_peripheral_cosim.rpt). No post-route result is implied.
The complete peripheral includes the command engine; their resource counts must
not be added as if they were separate instantiated blocks.

New regression coverage:

- Native peripheral: alignment/strobe errors, identity/rate, publication, active
  descriptor readback, cancellation status, actions, unsupported write errors.
- AXI transport RTL: independent W/AW arrival, absent halves, stable stalled
  responses, polling during stalls, SLVERR propagation, engine watchdog timeout.
- AXI plus actual generated HLS RTL: register/queue access, autonomous capture,
  cancellation and aborted completion, session recovery, and suppression of stale
  activation when protection arrives during an HLS transaction. Maximum observed
  read and write response: **198 clocks (1.98 us)**. Each request checks the proposed
  2,048-clock bound. These finite test sequences are not an exhaustive proof.

Integrated tests require freshly synthesized peripheral RTL; CTest does not run
Vitis automatically. `tools/integrated_rtl.py` fails if generated RTL is absent.
SmartHLS, full motion executor, motor tuning configuration, IRQs,
clock/power-loss board protection, and vehicle testing remain unverified.


## Safety/configuration integration — revision 0.12

The HLS peripheral now owns staged timeout and maximum speed, validates disabled/
closed/empty commit, increments configuration generation, and decodes watchdog
refresh and recovery intents. The RTL wrapper checks these intents again against
live protection state when HLS retires, before returning AXI status. Selected local
fault acknowledgment uses a mask; live/new causes retain priority.

Native configuration tests cover bounds, commit preconditions, authorization,
session/sequence checks and acknowledgment masks. Generated-RTL integration now
includes actual configuration, enable, accepted/repeated refresh, a 1 ms watchdog
expiry, shared HALT/output inhibition, rejected late refresh, and acknowledgment
without restart. The full 13-test regression passed; the integrated test takes
approximately 26 seconds in Icarus. Independent RTL tests cover expiry/refresh
same-clock priority and multi-motor escalation.

Vitis C simulation, synthesis, and RTL co-simulation passed for the updated
peripheral at 100 MHz. Updated HLS estimates are **36–306 cycles**, **14,470 LUT,
4,001 FF, 2 BRAM_18K, zero DSP**, and **7.074 ns** estimated clock path (10 ns target,
2.70 ns uncertainty). Current reports replace the earlier peripheral evidence;
previous revision numbers above describe historical runs. Maximum observed AXI
read/write response in the updated integrated sequence is **199 clocks (1.99 us)**.

No post-route timing or board behavior is established. The motion executor, PI/PWM
connection, full tuning/scaling configuration, telemetry/interrupts, and SmartHLS
remain pending. ENABLE means authorized state, not verified physical motion.

## C++ HLS motion executor — revision 0.13

Vitis 2026.1 C simulation, synthesis, and Verilog RTL co-simulation passed for
`dmotor_motion` on `xc7z020clg400-1` at 100 MHz. The test exercises all six command
types, reversal, settling and timeout priority, completion retention, feedback
qualification, authorization loss, closed-loop takeover, skipped control ticks,
relative-position overflow, and fractional ramp/integral accumulation.

HLS estimates: **43–189 cycles (0.43–1.89 us)**, initiation interval 44–190 cycles,
**18,749 LUT, 13,823 FF, 95 DSP, no BRAM**, and estimated **7.274 ns** clock path
against a 10 ns target with 2.70 ns uncertainty. Verilog co-simulation observed
39–185-cycle latency. See [synthesis](evidence/vitis_motion_csynth.rpt) and
[co-simulation](evidence/vitis_motion_cosim.rpt). These are component estimates,
not placed/routed timing or a proof of complete-controller fit.

The executor contains its own PI with fractional residual retention; the older
standalone PI is not an additional required instance. Nevertheless, simply adding
the peripheral and motion estimates gives 33,219 LUT per motor, before wrappers.
Duplicating those estimates exceeds the selected device's 53,200 LUT capacity.
Integrated synthesis and resource reduction are required before claiming two-motor
fit. The low control rate leaves scope for arithmetic sharing while retaining C++.

Native `motion_queue_test` checks ring-to-executor ordering, dwell, stop,
completion backpressure with continuing regulation, and cancellation without
repeated retirement. This validates a reference transaction sequence; the hardware
queue dispatcher has not been implemented.

The 15-test native/RTL regression passed, including the earlier generated peripheral
integration. Separately, the actual generated motion RTL passed with its autonomous
scheduler and logical PWM adapter: completion acknowledgment, measured 50% duty
across a full 5,000-clock PWM frame, mid-transaction inhibition, and sticky invalid
feedback fault. Maximum observed scheduler latency in this adapter sequence was
**110 clocks (1.10 us)**; this sequence does not exercise the kernel's longest path.
See [captured adapter result](evidence/motion_rtl.log) and
[regression log](evidence/native_rtl_tests.log). CMake now registers this adapter
as the sixteenth test when generated motion RTL is available. Icarus needs more
than the initial 60-second wall-clock test limit; its limit is now 180 seconds.
This simulation runtime is unrelated to the measured hardware latency.

Motor feedback is synthetic. Tests establish neither 1 RPM loaded performance nor
physical position accuracy. Encoder velocity estimation, tuning/configuration MMIO,
AXI-to-motion dispatch, shared-HALT integration for new executor faults, telemetry,
IRQs, SmartHLS, CDC/placed-route checks, and board trials remain open. Existing
independent safety and watchdog protection must remain active during all motion.

## C++ HLS velocity estimator — revision 0.14

Vitis 2026.1 C simulation, synthesis and Verilog RTL co-simulation passed for
`dmotor_velocity` at 100 MHz on xc7z020clg400-1. The shared native/HLS testbench
assesses every 1 ms snapshot from 100 through 400 ms at ideal 1/5/30/130 RPM in
both directions against the proposed 2% error bound. It also checks zero timeout,
restart qualification, reversal/return, invalid-transition recovery, missed ticks,
configuration changes and incoherent snapshots. This is ideal sensor arithmetic
validation, not a loaded motor or closed-loop control test.

HLS estimates: **2–85 clocks (0.02–0.85 us)**, initiation interval 3–86 clocks,
**5,065 LUT, 4,599 FF, 24 DSP, no BRAM**, estimated clock path **7.040 ns** against
10 ns with 2.70 ns uncertainty. See [synthesis](evidence/vitis_velocity_csynth.rpt)
and [RTL co-simulation](evidence/vitis_velocity_cosim.rpt). No post-route result
is implied. Current separate motion and estimator estimates total 119 DSP per motor;
two copies alone exceed the device's 220 DSPs. Arithmetic sharing/resource reduction
and integrated synthesis remain necessary for multi-motor deployment.

The small RTL event collector independently passes event counting, hidden reversal,
invalid-event exclusion and fault-latching tests. It captures state only; speed
arithmetic remains C++ HLS. The decoder/collector/HLS chain has not yet been
verified together in generated RTL. Fault propagation, coherent telemetry and
shared configuration commit also remain to be wired.

The estimator invalidates speed while acquiring a new period after startup,
timeout or reversal. The current executor faults on invalid closed-loop feedback;
a bounded acquisition policy must be implemented before directly connecting them.
See [the estimator contract](velocity_estimator.md). No successful motor startup
or reversal with the combined controller is claimed by this revision.

The complete **18-test** CTest regression passed, including both existing generated-RTL
integration tests and the new estimator/collector tests. See the retained
[regression log](evidence/native_rtl_tests.log). The generated estimator RTL was
validated separately by Vitis co-simulation with the shared C++ testbench.

## Combined HLS controller — revision 0.15

`dmotor_controller` combines the estimator and motion executor with bounded
feedback acquisition. The native and Vitis-shared testbench passes startup
qualification, duty-capped acquisition, reversal, delayed command acceptance,
invalid-transition faults, non-extensible acquisition deadlines, a valid sample
arriving exactly at expiry, stop during acquisition, authorization loss, capture
faults and acquisition configuration checks. Synthetic encoder data exercises
controller state transitions; no motor/vehicle dynamics are simulated here.

Vitis 2026.1 C simulation, synthesis and Verilog RTL co-simulation passed at
100 MHz on xc7z020clg400-1. HLS estimates **49–288 clocks (0.49–2.88 us)**,
50–289-clock initiation interval, **28,278 LUT, 21,909 FF, 151 DSP, no BRAM**,
and **7.277 ns** estimated clock path against 10 ns with 2.70 ns uncertainty.
Co-simulation observed 45–207-clock latency for this test sequence. See
[synthesis](evidence/vitis_controller_csynth.rpt) and
[co-simulation](evidence/vitis_controller_cosim.rpt).

These numbers already include estimator and motion arithmetic; do not add their
standalone estimates again. The combined kernel alone consumes 151 of the device's
220 DSPs, so two instances cannot fit without optimization. Integrated fit and
placed/routed timing are still unverified. The existing peripheral is a separate
required block until queue integration changes that partition.

The standalone motion kernel was also regenerated and passed C simulation,
synthesis and RTL co-simulation. Its external protocol remains strict about
invalid feedback. Current estimates are 43–189 clocks, 18,817 LUT, 13,824 FF and
95 DSP, replacing the earlier motion report. The small resource change does not
imply a standalone latency or behavior change.

This resolves the C++ estimator/executor qualification mismatch identified in
revision 0.14. It does not yet connect controller fault bits to physical/shared
HALT, dispatch AXI-ring commands at control boundaries, or replace the standalone
motion adapter with a combined generated-RTL adapter. Those integration tests,
motor-model validation, tuning MMIO, resource reduction and SmartHLS remain open.

The complete **19-test** native/RTL CTest regression passed against regenerated
standalone motion RTL, including its scheduler/PWM test and the existing
AXI/peripheral integration test. See [the retained log](evidence/native_rtl_tests.log).
The new combined controller was separately verified by Vitis RTL co-simulation;
it has not yet been exercised inside a complete AXI/safety/PWM wrapper.

## Shared queue/controller HLS dispatcher — revision 0.16

The dispatcher now synthesizes a single ring owner with the combined controller.
Current-tick offer/accept/capture avoids early activation while feedback is
unqualified. Native and Vitis testbenches pass exact activation/terminal timestamps,
ordered velocity/dwell/stop execution, full completion pressure with continuing
regulation, stop cancellation without duplicate retirement, drained reopen, rejected
descriptors, newly invalid feedback, missing ticks and between-tick capture faults.

Vitis 2026.1 C simulation, synthesis and Verilog RTL co-simulation passed on
xc7z020clg400-1 at 100 MHz. HLS estimates **42–447 clocks (0.42–4.47 us)**,
43–448-clock initiation interval, **41,862 LUT, 27,841 FF, 151 DSP, 2 BRAM_18K**,
with **7.277 ns** estimated path against 10 ns and 2.70 ns uncertainty.
Co-simulation observed 41–206-clock latency in this sequence; it does not exercise
every synthesized worst-case path. The linked reports now reflect revision 0.17 at depth 4; the figures above record the depth-16 baseline. See [synthesis](evidence/vitis_dispatcher_csynth.rpt)
and [co-simulation](evidence/vitis_dispatcher_cosim.rpt).

The estimate already includes ring, controller, estimator and PI. It consumes
approximately 78% of the device's LUTs before the remaining wrappers/configuration;
physical fit is not established and two instances require substantial resource
reduction. The existing peripheral's private ring must be replaced when adapting
AXI, not added to this dispatcher as another queue owner. The older peripheral's
199-clock observed AXI response does not describe this new internal service.

Controller configuration decoding and result encoding were extracted into shared
C++ helpers. The standalone controller was regenerated and again passed C simulation,
synthesis and RTL co-simulation with unchanged aggregate estimates. Existing ring
mutation behavior was retained; the new offer method is read-only.

The HLS dispatcher is not yet connected to the AXI/safety/scheduler/PWM wrappers.
Current tests do not prove physical HALT propagation, bus response latency under
tick arbitration, full configuration recovery, motor dynamics, SmartHLS or routed
resource/timing closure. Those remain explicit integration gates.

The full **20-test** native/RTL regression passed. A final dispatcher regression
also passes for a controller-side range fault on an offered command: no ring
activation, no committed acceptance telemetry and no spurious completion. The
updated dispatcher passed all Vitis stages again. See [test evidence](evidence/native_rtl_tests.log).


## Shared ring capacity — revision 0.17

`include/dmotor/ring_config.hpp` now owns the submission/completion capacity.
The default is four entries; arrays, masks and generated ABI constants derive
from it. Schema 4 reflects the changed default capacity. Descriptor width stays
16 words and completion width stays eight words.

Native suites pass at depths 2, 4 and 16, including counter wrap, full-ring
backpressure, cancellation and physical completion-slot reads after wrap.
Reproduce with `python3 tools/check_ring_depths.py`.

At depth 4, Vitis synthesis estimates the dispatcher at **38,019 LUT, 27,141 FF,
151 DSP and 2 BRAM_18K**, with **41–346 clocks** latency at 100 MHz. Relative to
the depth-16 baseline above this saves 3,843 LUT and 700 FF; DSP and BRAM counts
are unchanged. These are HLS estimates, not routed utilization or timing closure.

All 20 native/RTL tests pass at the default depth, including generated peripheral
and motion RTL integration. Command engine, peripheral and dispatcher each pass
Vitis C simulation, synthesis and Verilog co-simulation at depth 4. Updated
reports are in `docs/evidence/`; the alternate-depth native results are recorded
in `docs/evidence/ring_depth_tests.log`.

## Named timeout limits

The velocity/controller timing limits now use constants from
`include/dmotor/timing.hpp`, derived from the fabric clock. Timeout configuration
and expiry behavior are unchanged. All 11 native tests pass; velocity, controller
and dispatcher pass Vitis C simulation, synthesis and Verilog co-simulation.
Their aggregate LUT, FF, DSP and BRAM estimates are unchanged.

## Ramp arithmetic experiment

Compared at 100 MHz on xc7z020clg400-1 using Vitis 2026.1:

| Motion kernel | LUT | FF | DSP | BRAM_18K |
|---|---:|---:|---:|---:|
| Original 64-bit ramp accumulator | 18817 | 13824 | 95 | 0 |
| Cached whole/fractional rates (discarded) | 19689 | 14718 | 95 | 0 |
| Bounded 32-bit accumulator | 18788 | 13806 | 95 | 0 |

Caching the two rate decompositions added more hardware than it removed
([discarded experiment report](evidence/ramp_cached_motion_csynth.rpt)). The
retained change instead bounds the accumulation to 32 bits and preserves exact
division/modulo behavior. The motion kernel saves 29 LUT (0.15%) and 18 FF
(0.13%); this is a small improvement, not a solution to overall controller area.
These are HLS estimates, not placed-and-routed resource or timing measurements.

The extended motion test compares actual top-level results against the original
64-bit ramp calculation for 600 ticks covering fractional rates, maximum rates,
rate changes, reversals, clamping and reset, in addition to the existing tests.

The final motion estimate is 43–188 clocks (previously 43–189), with unchanged
95 DSP and 0 BRAM. The combined controller is 28243 LUT / 21880 FF (down 35 / 29),
and the dispatcher is 37991 LUT / 27114 FF (down 28 / 27), with unchanged DSP/BRAM
counts. Dispatcher maximum estimated latency is 345 clocks, previously 346.

Final validation: motion, controller and dispatcher pass Vitis C simulation,
synthesis and Verilog co-simulation. All 20 native/RTL tests pass across the full
run and retry: the unchanged peripheral RTL test first exceeded its 60-second
wall-clock limit during concurrent vendor runs, then passed alone in 25.90 seconds.
Motion RTL integration passed in 95.40 seconds. The saved native/RTL log includes
both the initial timeout and successful retry.

## Exact arithmetic reuse for the four-motor target

The approved system target is at most four motors. Controller independence and
all timing, command, safety and numerical requirements remain unchanged.

| Kernel | Previous LUT / FF / DSP | Current LUT / FF / DSP | Max clocks, before → after |
|---|---|---|---|
| Motion | 18788 / 13806 / 95 | 18226 / 12677 / 47 | 188 → 532 |
| Controller | 28243 / 21880 / 151 | 26471 / 19418 / 71 | 287 → 634 |
| Dispatcher | 37991 / 27114 / 151 | 36212 / 24615 / 71 | 345 → 692 |

BRAM remains 0/0/2 respectively. Vitis 2026.1 estimates 7.080 ns for motion
and 7.193 ns for controller/dispatcher, against a 10 ns clock with 2.70 ns
uncertainty. These are synthesis estimates, not routed timing closure.
The dispatcher estimate is 6.92 us, below the 800 us control budget. This alone
does not establish an integrated MMIO response bound under arbitration.

The retained implementation shares exact division/rounding helpers through
platform-specific directives. Broad operation-allocation experiments reduced
DSP count further (as low as 26), but degraded timing and were discarded.
Per-tick configuration validation, full numeric ranges, rational encoder scaling,
1 kHz updates and independent protective logic remain unchanged.

Four copies still require an estimated 144848 LUT and 284 DSP, exceeding the
xc7z020's 53200 LUT and 220 DSP before wrappers. This is a computation-cost
improvement, not four-motor fit or an architectural change. LUT reduction remains
the larger outstanding resource task. Configuration preparation and further
proven numeric-width reductions remain follow-up opportunities.

Validation passed: all 20 native/RTL tests, including generated motion and
peripheral integration; Vitis C simulation, synthesis and Verilog co-simulation
for motion, controller and dispatcher. The UBSan-enabled deterministic comparison
matches every reply word across 16,384 motion updates against revision `d4abc0b`;
see `evidence/arithmetic_equivalence.log`. This trace supplements rather than
replaces the command, safety and timing testbenches.

## Consolidated descriptor validation

The integrated dispatcher now has one descriptor validator at the current-tick
acceptance boundary. Private implementation paths reuse its result through
controller/motion processing and ring capture. Public standalone entry points
still perform full validation; no public bypass flag or MMIO change is introduced.

| Dispatcher estimate at 100 MHz | Before | After |
|---|---:|---:|
| LUT | 36212 | 32223 |
| FF | 24615 | 24132 |
| DSP | 71 | 71 |
| BRAM_18K | 2 | 2 |
| Maximum latency, clocks | 692 | 674 |
| Estimated path, ns | 7.193 | 7.193 |

This saves 3989 LUT (11.0%). Four copies still exceed the xc7z020 LUT/DSP budget;
no four-motor fit or routed timing closure is claimed.

The dispatcher tests now cover every descriptor error, including relative-position
overflow after publication, normalization of malformed sequence/session completion
fields, and ring/configuration generation disagreement before the first controller
tick. That disagreement is rejected before activation; the previous path could
provisionally accept in motion and reject later at ring capture. Existing tests
retain completion backpressure, current-boundary feedback readiness, protective
priority and cancellation coverage.

Validation passed: all 20 native/RTL tests, including generated peripheral and
motion integration; Vitis C simulation, synthesis and Verilog co-simulation for
command engine, peripheral, motion, controller and dispatcher. Standalone
command/peripheral/motion/controller resource totals remain unchanged. The
16,384-update native motion equivalence trace still matches revision `d4abc0b`.

## Shared state updates and single result commit

At 100 MHz on xc7z020clg400-1, the retained dispatcher changes yield:

| Estimate | Before | After |
|---|---:|---:|
| LUT | 32223 | 27007 |
| FF | 24132 | 21058 |
| DSP | 71 | 71 |
| BRAM_18K | 2 | 2 |
| Maximum latency, clocks | 674 | 700 |
| Estimated path, ns | 7.193 | 7.193 |

This saves 5216 LUT (16.2%) and 3074 FF. Latency is 7.00 us at 100 MHz.
Four copies still exceed the device resource budget; this is an HLS estimate,
not mapped/routed fit or a four-controller integration result.

Measured isolated experiments: a separate non-inlined tick function increased
LUTs to 33938 and was discarded. Single result commit plus direct telemetry
masking reduced LUTs to 32041. Sharing completion/fault state-update functions
alone reduced LUTs to 27073; combining the two retained changes gives 27007.
The same state-update directives on standalone motion/controller builds produced
7.629 ns estimated paths, exceeding the 7.30 ns effective budget, so those
configurations were restored. Only the dispatcher gets the new directives.

The deterministic native comparison matches all 40 reply words across 1378
service calls against revision `a81357a`, including cancellation, authorization
loss, stop, backpressure, every descriptor rejection and live telemetry masking.
See `evidence/state_equivalence.log`.

Validation passed: all 20 native/RTL tests and dispatcher Vitis C simulation,
synthesis and Verilog co-simulation. Standalone motion/controller RTL was
regenerated with the restored, unchanged configuration; their prior resource
and timing estimates are reproduced. The integrated motion RTL test uses that
restored RTL. Existing safety, queue and timing behavior remains covered.

## Priority 3: narrow internal values

The retained implementation uses scalar width constraints after validation or
clamping. It narrows motion state and explicitly bounds PI error/gain operands;
full-width intermediate products, rounding, saturation, positions and timestamps
are preserved. No MMIO or architectural range changes are required. The exact
bounds are listed in `motion_executor.md`.

An initial C++ bit-field wrapper experiment was discarded: Vitis inferred extra
memories and greatly increased dispatcher area. The retained scalar form avoids
that representation. State narrowing alone measured 26,909 LUT, 20,795 FF and
71 DSP in the dispatcher; bounding the PI operands additionally removes five DSPs.

The native equivalence tools now compile the baseline revision's own headers and
source dependencies. This prevents a changed private representation from silently
being applied to both sides of a comparison. Against `beeed8a`, all 16,387 recorded
motion snapshots and 1,378 complete dispatcher replies match under UBSan. The
motion run includes a million-tick settling case, checking every tick for early
completion and the exact terminal tick. Boundary tests additionally exercise both
maximum speed signs, maximum opposite-sign PI error, retained integral saturation,
full-scale duty, and invalid full-width commands that would alias legal commands
if narrowed before validation. See `evidence/narrow_equivalence.log`.

Final Vitis 2026.1 synthesis estimates at 100 MHz, xc7z020clg400-1:

| Kernel | LUT before → after | FF before → after | DSP before → after | Max clocks before → after | Path ns before → after |
|---|---:|---:|---:|---:|---:|
| Motion | 18226 → 17193 | 12677 → 11950 | 47 → 42 | 532 → 534 | 7.080 → 7.040 |
| Controller | 26471 → 25406 | 19418 → 18743 | 71 → 66 | 634 → 633 | 7.193 → 7.193 |
| Dispatcher | 27007 → 26694 | 21058 → 20574 | 71 → 66 | 700 → 701 | 7.193 → 7.193 |

BRAM remains zero for motion/controller and two BRAM_18K for the dispatcher.
The dispatcher saves 313 LUT (1.16%), 484 FF and five DSP (7.04%). Its 701-cycle
worst estimate is 7.01 us, within the existing 80,000-clock compute deadline.
Estimated paths remain below the 7.30 ns effective HLS budget. These are synthesis
estimates, not routed timing closure. Four dispatcher copies still exceed both
the device LUT and DSP budgets; this step does not establish a four-motor fit.

Generated motion RTL confirms 47-bit integral, 20-bit settling, 3-bit opcode,
2-bit mode, 10-bit ramp remainder, 18-bit duty and 21-bit speed-cap storage.
The target retains 22 bits after signed profile-speed negation.

Validation passed: all 20 native/RTL tests, including regenerated motion RTL
integration, plus Vitis C simulation, synthesis and Verilog co-simulation for
motion, controller and dispatcher. Checked-in synthesis/co-simulation reports and
`evidence/native_rtl_tests.log` reflect this implementation.

## Priorities 4 and 5: preparation and selected serialization

Priority 4 prepares motion validation, braking/lookahead denominators, the integral
limit and acquisition-policy validity during reset or disabled configuration
changes in the integrated controller. Priority 5 uses a sequential canonical-word
configuration comparison and an exact two-phase serial divider for motion and
velocity. See `controller.md` for preparation ordering and fault behavior, and
`motion_executor.md` for the arithmetic contract. Public MMIO formats, numeric
ranges, fractional accuracy, independent HALT protection and clock rate are unchanged.

All decoded configuration words are now checked for changes while enabled, including
same-generation changes. A mismatch faults before activation. Disabled configuration
changes restart estimator qualification; they neither clear protective latches nor
hide simultaneous encoder diagnostics. Standalone motion keeps per-call validation.

The arithmetic regression checks all unsigned 8-bit dividend/nonzero-divisor pairs,
full-width boundaries, powers of two, and 100,000 deterministic random pairs against
native division under UBSan. Motion snapshots and dispatcher replies match revision
`de69d54` for legal preparation sequences. The dispatcher generation-mismatch test
now supplies its changed configuration on a disabled tick before enabling, so it
continues to test ring ownership independently of the new configuration guard.
Controller tests cover mutation of each of the 17 decoded words, disabled scaling
changes, requalification, sticky fault retention and concurrent decoder diagnostics.

Experiments: configuration preparation initially increased dispatcher area to
28,275 LUT. The first single-phase serial divider missed the effective timing budget
(7.674 ns); the retained two-phase version separates compare and subtract. A serial
PI gain multiplier reduced DSP use by five but added 595 LUT relative to the serial
divider version, so it was discarded. Native PI multiplications remain in place.

Final Vitis 2026.1 estimates at 100 MHz on xc7z020clg400-1:

| Kernel | LUT before → after | FF before → after | DSP | Max clocks before → after | Path ns before → after |
|---|---:|---:|---:|---:|---:|
| Motion | 17193 → 16697 | 11950 → 10455 | 42 | 534 → 914 | 7.040 → 7.054 |
| Velocity | 5065 → 4340 | 4599 → 3844 | 24 | 85 → 149 | 7.040 → 7.040 |
| Controller | 25406 → 25264 | 18743 → 16771 | 66 | 633 → 1003 | 7.193 → 7.212 |
| Dispatcher | 26694 → 26337 | 20574 → 18060 | 66 | 701 → 1197 | 7.193 → 7.138 |

DSP and BRAM totals are unchanged. The dispatcher saves 357 LUT (1.34%) and
2514 FF (12.22%). Its worst synthesis latency is 11.97 us against the 800 us
compute deadline. All estimated paths remain below the 7.30 ns effective budget.
Preparation/storage and comprehensive configuration checks offset part of the
arithmetic savings. These are HLS estimates, not placed/routed closure or a proven
integrated MMIO arbitration bound. Four independent dispatchers still exceed the
xc7z020 LUT/DSP budget.

Validation passed: all 21 native/RTL tests, including regenerated motion RTL with
the autonomous scheduler/PWM wrapper, plus Vitis C simulation, synthesis and
Verilog co-simulation for motion, velocity, controller and dispatcher. Updated
reports are in `evidence/`; `preparation_serial_equivalence.log` records the
16,387 matching motion snapshots and 1,379 matching dispatcher replies.
