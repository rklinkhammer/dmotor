# Verification record — 2026-10-11

The active profile is now 20 MHz. Earlier sections below are historical records
of their stated clock profiles; see the final section for current adoption results.

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

## Selective BRAM and code-wide LUT review

Reviewed the portable protocol/ring, command engine, AXI peripheral, configuration,
controller, motion, velocity, arithmetic and standalone PI code, their Vitis adapters,
and the RTL safety, timing, encoder, PWM and bus boundaries. The review uses current
HLS hierarchy/memory reports for resource attribution; it is not a placed/routed
whole-system utilization audit. The standalone PI primitive is not instantiated by
the motion controller.

Retained completion changes flatten cold records into one word-addressed memory
and avoid full-record copies for single-word MMIO reads. The submission ring already
uses BRAM. Preserve ordinary scalar state for hot control/protection data rather
than forcing every field into a memory. No HALT/watchdog state moved into BRAM.

Further retained arithmetic changes replace relative-position overflow detection
with one unsigned sum and sign-bit checks, and bound/reorder velocity products only
after their full-width validation. Preparation also bounds validated speed, CPR,
deceleration and integral-limit operands before multiplying. These changes are exact,
including extreme signed positions
and fractional CPR. Protocol tests compare overflow against the original signed-bound
oracle for boundary pairs and 10,000 randomized pairs. Completion tests compare every
word against bulk records across counter wrap, and reject invalid word indices and
retired counters without changing the output value. Velocity tests cover configured
scaling extrema, one/99/100 events, both signs, excessive speed and inputs that would
alias valid values if narrowed before validation. Motion tests check the corresponding
configuration aliases and cross-product limits; equivalence traces now vary rational
CPR across small, fractional and maximum supported values.

Measured experiments, each against the initial 26,337-LUT dispatcher unless noted:

| Experiment | Result | Decision |
|---|---|---|
| Flat completion storage and direct word reads | 26,076 LUT; 4 rather than 2 BRAM_18K | Retain |
| Sign-bit relative-position overflow check | 26,236 LUT | Retain |
| Bounded/reordered velocity products | 25,855 LUT; 49 rather than 66 DSP | Retain |
| Bounded preparation products, after completion/overflow/velocity changes | 25,493 → 25,215 LUT; 49 → 39 DSP | Retain |
| Sequential reserved-field descriptor checks | 26,632 LUT; 7.247 ns path | Discard |
| Force temporary configuration words into BRAM | 26,333 LUT; one extra BRAM_18K | Discard: four-LUT saving is too small |
| Simplify watchdog clock conversion explicitly | Peripheral remains 9,875 LUT | Discard: no measured benefit |

Direct storage bindings to private class members were rejected or ignored by Vitis;
none of those directives are retained. The completion-array result above uses
successful natural BRAM inference confirmed in the memory report.

Final Vitis 2026.1 estimates against revision `4c7b935`, at 100 MHz on
xc7z020clg400-1, with ring depth four:

| Kernel | LUT before → after | FF before → after | DSP before → after | BRAM_18K before → after | Max clocks before → after | Path ns before → after |
|---|---:|---:|---:|---:|---:|---:|
| Command | 6493 → 6239 | 1918 → 1743 | 0 → 0 | 2 → 4 | 64 → 94 | 7.074 → 7.074 |
| Peripheral | 9875 → 9816 | 3401 → 3268 | 0 → 0 | 2 → 4 | 96 → 125 | 7.074 → 7.074 |
| Motion | 16697 → 16300 | 10455 → 9942 | 42 → 32 | 0 → 0 | 914 → 913 | 7.054 → 7.181 |
| Velocity | 4340 → 3858 | 3844 → 3011 | 24 → 7 | 0 → 0 | 149 → 145 | 7.040 → 7.040 |
| Controller | 25264 → 24415 | 16771 → 15469 | 66 → 39 | 0 → 0 | 1003 → 999 | 7.212 → 7.212 |
| Dispatcher | 26337 → 25215 | 18060 → 16461 | 66 → 39 | 2 → 4 | 1197 → 1251 | 7.138 → 7.181 |

The dispatcher saves 1122 LUT (4.26%), 1599 FF (8.85%) and 27 DSP (40.91%),
trading two additional BRAM_18K for cold completion storage. Its maximum HLS
estimate is 12.51 us against the 800 us compute deadline. All estimated paths
remain below the 7.30 ns effective budget. The service memory report confirms a
32-word × 32-bit completion memory and a 64-word × 32-bit submission memory,
each using two BRAM_18K. See `evidence/vitis_dispatcher_memory.rpt`.
These estimates do not establish routed timing or an integrated MMIO arbitration
bound. Four dispatcher copies would still require 100,860 LUT, exceeding the
53,200 available on xc7z020; their 156 DSP alone would fit its 220 DSP budget.

All 21 native/RTL tests pass, including the regenerated peripheral and autonomous
motion RTL integrations (`evidence/native_rtl_tests.log`). Vitis C simulation,
synthesis and Verilog co-simulation pass for command engine,
peripheral, motion, velocity, controller and dispatcher. Equivalence against
`4c7b935` matches 16,387 motion snapshots and 1,379 complete dispatcher replies
(`evidence/bram_equivalence.log`). Ring depths 2, 4, 16 and 64 pass native/ABI
checks (`evidence/bram_ring_depths.log`).

Remaining candidates from the review:

- Measure narrower parameter-derived RTL scheduler/PWM counters in Vivado. This
  may yield modest savings, but HLS reports cannot establish the benefit in these
  separately integrated RTL modules. Keep watchdog and HALT response independent.
- Profile remaining motion arithmetic and mux/control logic after synthesis and
  placement before choosing another serialization boundary. More serialization
  can add multiplexers and state; the discarded reserved-field experiment shows
  that fewer simultaneous comparisons need not mean fewer LUTs.
- If four motors must fit this device, evaluate one scheduled arithmetic engine
  with per-motor context and independent safety/PWM/encoder logic. This changes
  arbitration, latency verification and the integration architecture; it is not
  implemented by this optimization pass.

Full-width MMIO, ring counters, positions and timestamps are retained. No numeric
range, fractional precision, control algorithm, safety priority or clock-rate
requirement needs to change for the retained optimizations.

## Standalone PI narrowing experiment

The standalone `dmotor_pi` in `src/control.cpp` now exposes validated operand
bounds to HLS: 22 signed error bits, 21 unsigned gain bits, 17 unsigned period
bits, 42 signed proportional-product bits and 59 signed integral-product bits.
The rounded-division helper uses bounded unsigned magnitudes, then restores the
sign, retaining ties-away-from-zero rounding. The magnitude masks are applied
only after full-width validation; no valid product reaches the signed minimum
of its chosen width. Public Q16 configuration, accepted ranges, saturation,
anti-windup and invalid-input results are unchanged.

Vitis 2026.1 estimates against revision `4e8c79a`, xc7z020clg400-1 at 100 MHz:

| Metric | Before | After |
|---|---:|---:|
| LUT | 3370 | 2642 |
| FF | 3420 | 1737 |
| DSP | 56 | 24 |
| BRAM_18K | 0 | 0 |
| Latency, clocks | 18 | 13 |
| Initiation interval, clocks | 19 | 14 |
| Estimated path, ns | 7.014 | 7.017 |

This saves 728 LUT (21.60%), 1683 FF (49.21%) and 32 DSP (57.14%). The
constant-division multiplier instances now have 84- and 118-bit results rather
than the previous pair of 129-bit results. No fixed-point scale change or serial
divider replacement was needed. These are HLS estimates, not routed results.

The testbench compares against the original wide implementation, including both
outputs and the range fault. It exercises signed rounding around half units,
maximum products, period extrema, integral saturation, anti-windup, 2048 seeded
random valid configurations and invalid values that would alias valid inputs if
masked before validation. Native testing passes under undefined-behavior
sanitization (`evidence/pi_narrowing_native.log`). Vitis C simulation, synthesis
and Verilog co-simulation pass all 2240 transactions; the updated reports are
`evidence/vitis_pi_csynth.rpt` and `evidence/vitis_pi_cosim.rpt`.
This is a standalone-kernel change: it does not reduce integrated
controller or dispatcher utilization because they use `MotionExecutor`'s PI.

### Interpreting the controller's step_impl hierarchy

The retained integrated controller report totals 24,415 LUT. The child-report
excerpts are retained in `evidence/controller_hierarchy.rpt`. Its
`step_impl_1` instance (the controller method) totals 23,511 LUT **including its
children**; those numbers must not be added together. Within that instance:

| Child module | LUT |
|---|---:|
| Motion executor `step_impl` | 14646 |
| Velocity estimator `step` | 3345 |
| Configuration `prepare` | 1663 |
| Divider | 523 |
| Two loop pipeline modules | 160 |
| Parent-local expression, mux and memory logic | 3174 |

The motion executor itself contains 6659 expression LUT, 3740 multiplexer LUT
and 4247 child-instance LUT, including 2202 for descriptor validation. Thus the
largest integrated opportunity is motion arithmetic and selection/control logic,
not merely the short C++ wrapper or the standalone PI primitive. Selective
operator sharing and simplification of wide state/result selections should be
measured there; hierarchy totals alone do not prove which individual source
statement offers a saving. Synthesis may specialize these modules differently
inside the dispatcher, so retain top-level comparisons for each experiment.

## Named timing profile and protective budget

The approved external-HALT-to-FPGA-logical-inhibit budget is now **10 us**.
The previous 50 ns number described five clocks at the initial 100 MHz clock,
not a motor stopping time. The safety RTL still reacts immediately; no delay,
filter or relaxed fault priority has been introduced. The latency test checks
both the 10 us physical-time ceiling and the existing five-clock implementation
bound across ten input phases. Driver turn-off, shared-HALT propagation and
mechanical stopping remain separate integration measurements.

`abi/timing.json` defines the named control/PWM frequencies, control deadline,
HALT budget/pipeline bounds, MMIO cycle budget, encoder envelope, timeout limits,
standalone PI period limits and settling limit. `tools/generate_abi.py` generates
`include/dmotor/timing.hpp` and `rtl/timing.sv` together and checks Vitis clock
constraints against CLOCK_HZ in `abi/registers.json`. Compile `rtl/timing.sv`
before scheduler/PWM modules; the repository RTL test scripts do this.

The clock remains 100 MHz; numerical constants and behavior remain unchanged
except for the approved external-HALT acceptance budget. This is preparation for
clock experiments, not evidence of an operating lower-frequency implementation.
Changing CLOCK_HZ regenerates conversions, scheduler/PWM defaults and HLS clock
constraints. Rebuild all HLS RTL, update clock-dependent test fixtures and platform
clock constraints, then re-run integration/timing/CDC verification. The generator
requires integer MHz, exact control/PWM/event-rate divisors, representable interval
counters and enough clocks for the HALT pipeline. Arithmetic-envelope changes
are rejected where existing width proofs depend on them.

| Named quantity | Current value | Meaning |
|---|---:|---|
| control_hz / control_period_us | 1000 / 1000 us | Autonomous control cadence |
| control_deadline_us | 800 us | Computation deadline, before the next tick |
| pwm_hz / pwm_period_cycles | 20000 / 5000 | PWM frequency and frame length |
| external_halt_budget_us | 10 us | External HALT to logical inhibition |
| external_halt_pipeline_max_cycles | 5 | Separate implementation regression bound |
| synchronized_inhibit_max_cycles | 2 | Qualified input to logical inhibition target |
| mmio_response_max_cycles | 2048 | Local register transaction cycle budget |
| encoder_max_events_hz | 100000 | Supported event-rate envelope |
| encoder_max_interval_us | 10000000 us | Largest accepted measurement interval |
| watchdog_min_us / watchdog_max_us | 1000 / 10000000 us | Configurable authorization timeout bounds |

Scenario-specific test stimuli and numeric RPM/Q16/CPR scales are not hardware
timing-profile controls. Runtime command timeouts and dwell durations remain
programmed values in microseconds, with the existing public formats unchanged.

Validation: all 21 native/RTL tests pass at 100 MHz
(`evidence/timing_native_rtl_tests.log`). The RTL integrations use the previously
synthesized motion/peripheral kernels with unchanged arithmetic timing constants;
this refactoring does not claim a new HLS utilization measurement. Isolated
profile generation and safety RTL phase sweeps also pass at 100, 20, 10 and 1 MHz
(`evidence/timing_profile_checks.log`). These lower-clock checks cover only the
HALT synchronizer/inhibit path, not a complete lower-frequency controller.

## 20 MHz clock evaluation

Evaluated an isolated copy in `build/clock20` from revision `0eccb45`, with
CLOCK_HZ set to 20,000,000 and all timing/RTL/HLS constants regenerated. The
active source profile remains at 100 MHz. The only retained executable changes
are clock-independent test fixtures and `tools/prepare_clock_profile.py`.
Existing 100 MHz synthesis reports remain the baseline; new reports live in
`evidence/clock20/`.

The test fixtures now express 1 ms snapshots, 100 ms zero qualification, 60 ms
acquisition, watchdog conversions and ideal encoder timestamps using the selected
clock. Speed, acceleration, gain and microsecond command values are unchanged.
RTL test clocks also follow the profile. The encoder test holds each state for
exactly the minimum event interval; the generated-motion integration measures
50% duty over a complete profile-derived PWM frame.

At 20 MHz the 1 kHz tick is 20,000 clocks; the 800 us computation deadline is
16,000 clocks. A 20 kHz PWM frame is 1,000 clocks, giving 0.1% duty increments
versus 0.02% at 100 MHz. The 100 kHz encoder envelope permits 200 clocks per
transition. The five-clock local HALT implementation bound becomes 250 ns,
well inside the approved 10 us digital-inhibit budget. The MMIO contract remains
2,048 clocks, or 102.4 us at this clock; it is not a fixed 20.48 us requirement.

Lowering the clock does not automatically narrow the declared 64-bit timestamps
or RTL counters. HLS can schedule fewer pipeline registers and more work within
each clock; that can substantially reduce FF use without reducing LUTs. PWM
quantization, loaded 1 RPM operation, routed timing, CDC, shared HALT and the
motor-driver electrical response still require platform/integration validation.
No lower power consumption has been measured.

To reproduce in a new directory:

```sh
python3 tools/prepare_clock_profile.py --hz 20000000 --output build/clock20-repeat
cd build/clock20-repeat
cmake -S . -B build  # Supply Icarus paths if not on PATH.
cmake --build build -j4
python3 tools/vitis_check.py --kernel motion
python3 tools/vitis_check.py --kernel velocity
python3 tools/vitis_check.py --kernel controller
python3 tools/vitis_check.py --kernel dispatcher
python3 tools/vitis_check.py --kernel peripheral
cmake -S . -B build  # Discover the generated-RTL integration tests.
ctest --test-dir build --output-on-failure
```

These kernels cover the integrated arithmetic and register peripheral; the
standalone PI and command-engine tops were not separately resynthesized in this
experiment. The dispatcher is still not connected to the complete physical
AXI/controller/output path, so component bounds do not prove end-to-end arbitration.

Vitis 2026.1 synthesis comparison, same xc7z020clg400-1 target:

| Kernel | LUT 100 → 20 MHz | FF 100 → 20 MHz | DSP | BRAM_18K | Max clocks 100 → 20 MHz | Max time at 20 MHz |
|---|---:|---:|---:|---:|---:|---:|
| Motion | 16300 → 16835 | 9942 → 7327 | 32 | 0 | 913 → 752 | 37.60 us |
| Velocity | 3858 → 3746 | 3011 → 1942 | 7 | 0 | 145 → 132 | 6.60 us |
| Controller | 24415 → 24381 | 15469 → 10932 | 39 | 0 | 999 → 951 | 47.55 us |
| Dispatcher | 25215 → 25174 | 16461 → 12630 | 39 | 4 | 1251 → 1064 | 53.20 us |
| Peripheral | 9816 → 9490 | 3268 → 2699 | 0 | 4 | 125 → 119 | 5.95 us |

DSP/BRAM counts do not change. Dispatcher savings are only 41 LUT (0.16%),
but 3831 FF (23.27%). Motion alone grows by 535 LUT (3.28%). The dispatcher
uses 6.65% of the 800 us compute budget by the HLS worst-latency estimate,
excluding external arbitration/handshake overhead. Four independent dispatchers
still total 100,696 LUT against the device's 53,200 LUT; this clock change does
not solve four-motor capacity.

Estimated paths at 20 MHz are motion 32.316 ns, velocity 23.369 ns, controller
36.442 ns, dispatcher 34.854 ns and peripheral 14.220 ns. All meet the 36.5 ns
HLS effective budget (50 ns period minus 13.5 ns default uncertainty), although
the controller is close to that budget. These remain pre-route estimates.

Validation completed: Vitis C simulation, synthesis and Verilog co-simulation
pass for all five evaluated kernels. All 21 native/RTL tests pass at 20 MHz,
including generated peripheral and motion RTL integrations; see
`evidence/clock20/native_rtl_20mhz.log`. The updated fixtures also pass all 19
native/unit-RTL tests at the unchanged 100 MHz profile
(`evidence/clock20/native_rtl_100mhz.log`; generated-RTL integrations were not
rerun at 100 MHz in this experiment).

The 20 MHz AXI integration observed maximum write/read responses of 99/103
clocks (4.95/5.15 us). The motion integration observed a maximum of 223 clocks
(11.15 us) for its finite open-loop/fault scenario, verified 50% duty over the
1,000-clock PWM frame, and rejected stale output after mid-flight inhibit.
That observed motion latency does not replace the 752-clock synthesis worst
bound. HALT passed ten input phases against both 250 ns pipeline and 10 us
physical-time ceilings. Encoder X1/X2/X4 forward/reverse tests passed with a
200-clock (10 us) transition interval.

Conclusion: 20 MHz is viable for the tested component timing, with much lower
FF use but negligible integrated LUT savings. Keep the production profile at
100 MHz pending an explicit adoption decision and platform validation. Further
LUT work should target motion arithmetic widths and mux/control structure,
rather than expecting a lower fabric clock alone to solve capacity.

## Arithmetic and control-logic LUT experiments

Eight isolated experiments evaluated the active 100 MHz profile against
`8494e86` (25,215 dispatcher LUT, 16,461 FF, 39 DSP, four BRAM_18K,
1,251 worst-case clocks, 7.181 ns estimated path). All use the same Vitis 2026.1
xc7z020clg400-1 target. Candidate patches, dispatcher synthesis reports and a
machine-readable table are retained in `evidence/lut_review/`; patches apply to
that reference revision. No result here changes the clock, accepted motor ranges,
MMIO layout, rounding or safety priorities.

| Experiment | Dispatcher LUT | Delta | DSP | Max clocks | Path ns | Decision |
|---|---:|---:|---:|---:|---:|---|
| Braking operands plus PI intermediate/result narrowing | 25481 | +266 | 33 | 1119 | 7.296 | Reject for LUT objective |
| Shared position subtraction plus reuse accepted PI effort | 25332 | +117 | 39 | 1250 | 7.181 | Reject |
| Braking operands alone | 25468 | +253 | 33 | 1118 | 7.181 | Reject for LUT objective |
| Reuse accepted PI effort alone | 25332 | +117 | 39 | 1250 | 7.181 | Reject |
| Compare timeout in cycles; divide only for dwell | 25804 | +589 | 39 | 1122 | 7.181 | Reject |
| Inline rounded helper to share profile/PI divider | 25161 | -54 | 39 | 1247 | 8.134 | Reject: exceeds 7.30 ns effective timing budget |
| Mask each internal fault update | 25178 | -37 | 39 | 1251 | 7.181 | Superseded by entry normalization |
| Bound motion faults and normalize controller faults once at entry | 24915 | -300 | 39 | 1250 | 7.181 | Retain |

The retained change exposes the only internally generated motion fault bits
(configuration, tick/session, timeout, feedback) and controller bits
(configuration, capture, decoder, estimator, acquisition, motion) as named masks.
Motion applies its mask when latching a cause; the controller normalizes its
state once at entry, before applying fault causes. This exposes the state bound
even on calls with no new fault; masking only updates left a wide feedback path.
Every supported sticky bit is retained until the existing reset/recovery logic
clears it. Public fault words remain 32 bits. External physical-fault buses
are not masked by this change. Adding an internal fault cause must extend its
corresponding supported mask as well as the fault definition and tests.

The retained saving is 300 LUT (1.19%) and 364 FF (2.21%); DSP and BRAM are
unchanged. Worst estimated latency falls by one clock to 1,250 clocks (12.50 us). The remaining design is still far above a four-copy LUT
fit. Narrowed arithmetic reduced DSP demand but increased LUT demand through the
resulting synthesis schedule/binding; it is an alternative for DSP pressure,
not a LUT improvement. Source width and expression count alone are insufficient
predictors of integrated area.

Native boundary tests passed for both initial candidates. Full motion and
dispatcher traces for isolated braking, duty and timeout candidates matched the
baseline under UBSan (`candidate_traces.log`; dispatcher output includes one
non-record success line). The retained masks separately match all 16,387 motion
snapshots and 1,379 dispatcher replies against `8494e86`. Rejected experiments
were not all taken through RTL co-simulation; their rejection follows area or
synthesis timing results, not a claim of fully qualified RTL behavior.

Final synthesis of the retained code confirms four-bit motion fault state and
six-bit controller fault state in the generated dispatcher hierarchy:

| Kernel | LUT before → after | FF before → after | Max clocks before → after | Path ns before → after |
|---|---:|---:|---:|---:|
| Motion | 16300 → 16300 | 9942 → 9942 | 913 → 913 | 7.181 → 7.181 |
| Controller | 24415 → 24073 | 15469 → 15143 | 999 → 999 | 7.212 → 7.241 |
| Dispatcher | 25215 → 24915 | 16461 → 16097 | 1251 → 1250 | 7.181 → 7.181 |

The main resource reports reflect this retained code; experimental reports
remain separate. These are pre-route HLS estimates. The controller's 7.241 ns
path is within, but close to, the effective 7.30 ns budget. Four dispatcher
copies still require 99,660 LUT before surrounding integration logic.

Final verification passed: all 21 native/RTL tests, including regenerated motion
RTL with the scheduler/PWM adapter, and Vitis C simulation, synthesis and Verilog
co-simulation for motion, controller and dispatcher. The final controller/dispatcher
flows verify entry normalization; motion RTL is unchanged by that follow-up.
See `evidence/native_rtl_tests.log`, the updated top-level synthesis/co-simulation
reports and `evidence/lut_review/retained_equivalence.log`.


## Combined dividend/quotient register experiment

Against `0ea8f180f06d64d65e5808d7eef5f84174105daf`, the exact unsigned
64-bit divider now shifts dividend bits out and quotient bits into the same
register. The high dividend bit is consumed in the comparison phase; the shared
register shifts and appends the quotient bit in the subtraction phase. After
64 iterations it contains the quotient. The full-width remainder carry and
nonzero-denominator precondition are unchanged. There is no change to rounding,
accepted ranges, MMIO, clock profile or control/safety requirements.

Vitis 2026.1 estimates at 100 MHz on xc7z020clg400-1:

| Unit | LUT before → after | FF before → after | Max clocks before → after | Path ns before → after |
|---|---:|---:|---:|---:|
| Divider core | 523 → 450 | 206 → 142 | 130 → 130 | 6.589 → 6.589 |
| Motion | 16300 → 16166 | 9942 → 9810 | 913 → 915 | 7.181 → 7.290 |
| Velocity | 3858 → 3785 | 3011 → 2883 | 145 → 145 | 7.040 → 7.040 |
| Controller | 24073 → 23866 | 15143 → 14995 | 999 → 1001 | 7.241 → 7.244 |
| Dispatcher | 24915 → 24774 | 16097 → 15946 | 1250 → 1252 | 7.181 → 7.244 |

The core saves 73 LUT (13.96%) and 64 FF (31.07%). Its expression selections
fall by 64 LUT and additional multiplexers by nine LUT. There are still two
physical divider instances in the dispatcher. Integrated dispatcher savings are
141 LUT (0.57%) and 151 FF (0.94%), rather than simply twice the core saving,
because surrounding scheduling and binding also change. DSP and BRAM counts
are unchanged. Maximum dispatcher latency is 12.52 us, within the existing
800 us computation budget. Four dispatcher copies still exceed device LUT
capacity; this experiment does not establish a four-motor fit.

These are pre-route HLS estimates. All four kernels meet the 7.30 ns effective
synthesis budget, but standalone motion has only 0.010 ns margin and dispatcher
has 0.056 ns. Physical implementation must establish timing closure. Baseline
reports and equivalence evidence are in `evidence/divider_register/`.

Retained after verification: all 21 native/RTL tests passed, including the
regenerated motion kernel with scheduler/PWM integration. Vitis C simulation,
synthesis and Verilog co-simulation passed for motion, velocity, controller and
dispatcher. Native divider boundary/random tests and all 16,387 motion snapshots
and 1,379 dispatcher replies match the reference under UBSan. See
`evidence/divider_register/native_rtl_tests.log`, `equivalence.log` in that same
directory, and the updated top-level synthesis/co-simulation reports.


## Active 20 MHz profile adoption

The user-selected active fabric frequency is now 20 MHz, retaining the combined
dividend/quotient divider and all previous functional optimizations. This adopts
a new profile in the main project, rather than the earlier isolated experiment.
`abi/registers.json` owns CLOCK_HZ; generated register/timing headers, RTL packages,
register documentation and all seven Vitis configurations were regenerated.
`abi/timing.json` retains the time-based requirements.

| Property | Active value |
|---|---:|
| Fabric clock period | 50 ns |
| Control interval | 20,000 clocks / 1 ms |
| Computation deadline | 16,000 clocks / 800 us |
| PWM period | 1,000 clocks / 50 us |
| PWM duty resolution | 0.1% |
| External HALT logical inhibit budget | 200 clocks / 10 us |
| Local HALT pipeline bound | 5 clocks / 250 ns |
| Synchronized inhibit bound | 2 clocks / 100 ns |
| MMIO response budget | 2,048 clocks / 102.4 us |
| Minimum qualified encoder event spacing | 200 clocks / 10 us |

The local HALT bound is not a shared-network, driver turn-off or mechanical stop
bound. MMIO retains its cycle-based budget and consequently has a longer elapsed
time allowance. Timestamp clients must read CLOCK_HZ rather than assuming
100 MHz. These changes preserve the 1 kHz loop, 20 kHz PWM, 800 us deadline,
10 us external HALT budget and configured watchdog durations.

The reference `~/workspace/dvrmc` Vivado design was not modified. Its inspected
100 MHz fabric requires a real 20 MHz DMOTOR clock and synchronized reset, plus
AXI clock conversion when retaining the 100 MHz bus. See `vivado_reference.md`.
Physical implementation, CDC review and hardware measurements remain necessary;
HLS estimates and simulations do not establish those integration results.

Vitis 2026.1 HLS synthesis at 20 MHz on xc7z020clg400-1:

| Kernel | LUT | FF | DSP | BRAM_18K | Maximum clocks | Maximum time us | Estimated path ns |
|---|---:|---:|---:|---:|---:|---:|---:|
| PI | 1964 | 122 | 24 | 0 | 1 | 0.05 | 34.293 |
| Command | 6215 | 1694 | 0 | 4 | 92 | 4.60 | 10.997 |
| Peripheral | 9490 | 2699 | 0 | 4 | 119 | 5.95 | 14.220 |
| Motion | 16616 | 7135 | 32 | 0 | 752 | 37.60 | 32.316 |
| Velocity | 3673 | 1878 | 7 | 0 | 132 | 6.60 | 23.369 |
| Controller | 23793 | 10526 | 39 | 0 | 951 | 47.55 | 36.205 |
| Dispatcher | 24578 | 12107 | 39 | 4 | 1064 | 53.20 | 34.617 |

The effective HLS timing budget is 36.50 ns (50 ns minus 13.50 ns uncertainty).
All seven kernel estimates meet it. Dispatcher time remains below the 800 us
computation deadline. Against the preceding combined-divider 100 MHz profile,
dispatcher LUT drops from 24,774 to 24,578 (196 LUT, 0.79%), and FF drops from
15,946 to 12,107 (3,839 FF, 24.07%). DSP and BRAM are unchanged. This is not a
four-copy fit or a routed timing result. Standalone motion LUT increases from
16,166 to 16,616, illustrating that slower clocks do not uniformly reduce LUT.

Validation passed: all 21 native/RTL tests at the active 20 MHz profile, including
regenerated peripheral/safety and motion/scheduler/PWM integration; generated ABI
consistency; and Vitis C simulation, synthesis and Verilog co-simulation for all
seven kernels. Updated `evidence/vitis_*_{csynth,cosim}.rpt` files now describe
this profile; historical measurements above remain labelled by their original
clock. Native/RTL results are in `evidence/native_rtl_tests.log`.


## Bounded position-error experiment at 20 MHz

Two implementations were measured against the active 20 MHz profile (24,578 LUT,
12,107 FF). The current architecture admits unsigned 32-bit position tolerances,
full signed 64-bit positions/targets, and braking thresholds above UINT32_MAX.
Simply saturating position error to INT32_MAX or UINT32_MAX would change some
completion/braking decisions. Both candidates therefore preserve full-range
behavior while exposing 32-bit local comparisons:

1. Low 32-bit distance plus a fits flag, with a wide braking fallback.
2. Low/high 32-bit parts, comparing upper parts before lower parts for braking.

| Dispatcher variant | LUT | Delta | FF | Maximum clocks | Path ns |
|---|---:|---:|---:|---:|---:|
| Existing wide comparisons | 24578 | 0 | 12107 | 1064 | 34.617 |
| Narrow common path / wide fallback | 24675 | +97 | 12041 | 1064 | 34.617 |
| Split upper/lower comparisons | 24639 | +61 | 12107 | 1064 | 34.617 |

Both alternatives retain 39 DSP and four BRAM_18K. Neither reduces LUT usage,
so neither is retained in production source. Patches and synthesis reports are
in `evidence/position_error/`. They apply to the current motion implementation
(the source at `c92dee35653bba5fed1d32e6c59a19171827ee77`) with the separately
adopted 20 MHz profile. No prior clock/profile edits were reverted.

Both variants match all 16,387 motion snapshots and 1,379 dispatcher reply
records against the pre-experiment sources at the same 20 MHz clock under UBSan;
trace logs include an additional dispatcher success line. The retained
`position_error_test` covers full signed-position endpoints, both directions,
zero and UINT32_MAX tolerances, distances across the 32-bit boundary, and
braking thresholds above 32 bits in both directions. These cases also guard the
planned rebase implementation. They test digital semantics, not motor dynamics.

### Next implementation: rebased local moves

The user selected rebasing with smaller locally executed moves as the next
step; it is not implemented by this experiment. Retain 64-bit externally visible
position and command targets, but establish a bounded local execution window
before replacing the wide subtraction itself. Define a local segment limit
with headroom for braking/lookahead, tolerance, feedback increments and reversal;
large braking thresholds must be handled explicitly rather than silently clipped.
Rebase the local position and target by the same amount and accumulate that
amount into the 64-bit offset at a coherent tick boundary. Encoder edges on the
boundary must be counted once, including reverse motion. Internal segment
boundaries must not publish completion or lose queued-command/session identity.
Preserve timeout, watchdog, HALT and overflow behavior. Validate continuity of
reported absolute position and position error through repeated positive/negative
rebases and segment transitions before measuring resource savings.

Both experimental dispatcher versions passed Vitis C simulation and Verilog
co-simulation. After restoring the original comparison source, all 22 native/RTL
tests pass, including the new position-error test and the generated motion and
peripheral integration tests. Restored dispatcher synthesis reproduces 24,578
LUT, 12,107 FF, 1,064 clocks and a 34.617 ns path. Experimental evidence remains
separate from the active top-level reports.
