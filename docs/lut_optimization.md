# LUT optimization — dmotor_mipi_opt

> Historical implementation/evaluation record. Main now uses the [1 ms PS executor](../DMOTOR_ARCH.md); current build and acceptance state are in [build.md](build.md) and [system_status.md](system_status.md). HLS motor sizing/build instructions below do not select the current product.

Base commit: `67cb5dad1cdfeaf27eed65bb435150134d818f29`.
Sandbox: `/home/rklinkhammer/workspace/dmotor-mipi-opt`.
Measurements: Vivado/Vitis 2026.1, XC7Z020, 2026-11-06.

The retained implementation uses **37,795 LUTs in complete-controller OOC
synthesis**, versus 39,906 for the matched baseline: **2,111 fewer LUTs (5.29%)**.
After routing, LUTs fall from **39,526 to 37,342 (5.53%)**. It consumes five
additional DSPs and no additional BRAM. Occupied slices rise to **99.62%**, so this
does not establish usable capacity for MIPI or other sensor IP. The original dmotor
checkout is unchanged. This sweep does not establish a global optimum.

## Scope and contracts

Module ports, MMIO/HLS layouts, numerical precision, accepted ranges, control/PWM
rates, coherent snapshots, configuration commits, fault handling, STOP/rearm and
timing requirements are preserved. All application daemon access still belongs
through dmotor_client. No sensor IP or MIPI interface is introduced despite the
exploratory branch name.

Hardware testing remains paused pending assembled monitor and motor boards.
Commissioning-first scope is unchanged. Cleanup/prepare tests do not implement
or establish live monitor recovery or automatic mission resumption.

## Retained changes

1. Parameter-derived counter widths in PWM, scheduler, transports, cleanup and
   heartbeat. Widths include terminal increments where necessary; public latency
   and status fields remain full width. PWM magnitude is 17 bits after the existing
   full-signed-input saturation. Free-running timestamps, encoder counts, sequence
   numbers and target-age arithmetic retain their original widths.
2. Fixed word decoding in both HLS transports, preserving synchronous reads,
   out-of-range zero, dual-write priority, finalization and response backpressure.
3. `AreaOptimized_high` applied only to the controller OOC synthesis run in both
   Arty-Z7-20 and DVRZ project creation. This is the synthesis directive alone,
   not the broader `Flow_AreaOptimized_high` strategy. Clocks and implementation
   constraints are unchanged. Both project-generation checks verify the property.

Default HLS storage and source remain unchanged. All 41 prepared HLS input/config
files match the baseline byte for byte; see [source comparison](evidence/lut_optimization/final_hls_source_match.json).
The validated existing atomic/motor HLS RTL was reused for the retained build.
Board generation is not warning-free: Arty reports negative DDR DQS-to-clock
skew values; DVRZ import reports a missing PS `IRQ_F2P` pin and ties the unconnected
`controller_axi_0/hb_async` input low. The tracked board definitions are unchanged
by this branch. These are integration limitations to resolve before board use;
a successful project-generation exit is not hardware acceptance.

The entire `tools/build.py` product pipeline was not rerun; constituent native,
RTL, actual-HLS integration, IP export and board-project creation checks were run.

## Matched synthesis measurements

All rows measure the complete controller, with the same generated atomic core.
Only the named motor-HLS experiment changes that core's inputs. Except for `area`,
all rows use the Default synthesis directive. Storage/compact rows include the
counter and decoder changes. Do not sum HLS estimates with these totals or
compare them directly to the historical 41,203-LUT placed-board report.

| Variant | LUTs | FFs | DSPs | BRAM tiles |
| --- | ---: | ---: | ---: | ---: |
| Baseline | 39,906 | 56,036 | 16 | 2 |
| Bounded counters/PWM | 39,558 | 55,854 | 21 | 2 |
| Counters + word decoders | 39,150 | 55,855 | 21 | 2 |
| **Retained: counters + decoders + area directive** | **37,795** | **55,847** | **21** | **2** |
| Packed contexts, BRAM | 39,935 | 52,443 | 21 | 40 |
| Packed contexts, LUTRAM | 39,523 | 52,182 | 21 | 2 |
| Compact status, BRAM | 39,724 | 51,343 | 21 | 37.5 |
| Compact status, LUTRAM | 40,186 | 52,275 | 21 | 2 |
| Compact status, registers | 39,801 | 56,014 | 21 | 2 |

[Machine-readable results](evidence/lut_optimization/results.json), per-variant
utilization reports in the same directory, and [input hashes](evidence/lut_optimization/inputs.json)
record the evidence. Archived text reports trim trailing whitespace only. Narrower PWM arithmetic changes DSP mapping: the reduction
is a resource tradeoff, not free logic removal.

## Routed implementation

Matched baseline and retained-controller routing uses the unchanged
`constraints/controller_halt.xdc` and 50 ns (20 MHz) controller clock.

| Routed metric | Baseline | Retained |
| --- | ---: | ---: |
| LUTs | 39,526 | 37,342 |
| FFs | 56,036 | 55,847 |
| Occupied slices / 13,300 | 13,228 (99.46%) | 13,250 (99.62%) |
| DSPs | 16 | 21 |
| BRAM tiles | 2 | 2 |
| Worst setup slack | +9.778 ns | +7.427 ns |
| Worst hold slack | +0.063 ns | +0.090 ns |
| Total setup/hold negative slack | 0 / 0 ns | 0 / 0 ns |
| DRC errors | 0 | 0 |

Both completed the scripted timing/DRC error gates. The retained design saves
**2,184 routed LUTs**, but uses **22 more occupied slices**, leaving only 50 wholly
unoccupied slices in this OOC placement. Its setup margin is 2.351 ns smaller while
still meeting the specified clock. The result meets the LUT objective; it is not
a solution to overall fabric congestion or a MIPI capacity reservation.

[Machine-readable routed results](evidence/lut_optimization/routed_results.json),
[baseline utilization](evidence/lut_optimization/baseline_routed_utilization.rpt),
[retained utilization](evidence/lut_optimization/area_routed_utilization.rpt), and
[retained timing](evidence/lut_optimization/area_routed_timing.rpt) record the result.
DRC reports retain DSP-pipelining and missing-PS warnings appropriate to this
standalone controller measurement. Vivado also warns that OOC port routes lack
physical locations and some connectivity DRCs are unavailable. Thus these timing
results qualify the constrained internal paths only. OOC routing does not qualify
physical board IO, full PS/PL integration, MIPI capacity or a deployable bitstream.

## Verification

- Native/RTL CTest: **36/36 passed**, including generated ABI checks and real atomic
  HLS ([log](evidence/lut_optimization/native_tests.txt)).
- Prepared-profile native tests passed against controller arithmetic/behavior
  oracles ([log](evidence/lut_optimization/profile_tests.txt)).
- PWM/heartbeat differential comparison: **150,000 cycles**, including period-one,
  non-power-of-two/power-of-two periods, extreme signed duty, resets/inhibition
  ([log](evidence/lut_optimization/equivalence.txt)).
- Transport differential comparison: **5,000 cycles per adapter**, every selector,
  same-address dual writes, CE gating, timeout, completion and backpressure
  ([log](evidence/lut_optimization/transport_equivalence.txt)).
- Actual-HLS AXI and interrupted-cleanup benches passed for the retained RTL;
  final IP export/host simulation and both board project creation checks passed
  ([integration checks](evidence/lut_optimization/integration_checks.txt)).
- Unchanged RTL and generated-HLS port declarations checked explicitly
  ([interfaces](evidence/lut_optimization/interface_checks.json)).
- All measured synthesis variants completed loop DRC without errors.

These are simulation and implementation checks, not a formal equivalence proof
or physical motor qualification. No board was programmed and no daemon was accessed.
The existing integration bench's phrase “explicit recovery” does not establish
live monitor recovery; the commissioning constraints above still apply.

## Rejected and deferred experiments

Packed context and compact-status candidates passed HLS C simulation and C/RTL
co-simulation over 2,449 interleaved services, plus both actual-HLS integration
benches. Compact status also matched all 16 reply words against the baseline over
2,449 services ([comparison](evidence/lut_optimization/native_reply_equivalence.txt)).
However, their complete-controller LUT totals were worse than the retained
candidate. HLS resource estimates alone were misleading for this objective.
Compact candidate routes were stopped after synthesis showed no LUT advantage;
no routed result is claimed for those variants.

`tools/prepare_shared_mailbox.py --context-storage registers|bram|lutram` retains
explicit experimental alternatives, with the original `registers` behavior as
default. BRAM/LUTRAM require the copy variant and modify only generated profile
sources. [Reproduction checks](evidence/lut_optimization/experimental_profile_reproduction.json)
confirm that these options recreate the measured packed-profile inputs.
The compact-status change is archived as
[an experimental patch](../experiments/lut_opt/compact_status.patch), not applied
to default sources. Combinations with the area directive were not exhaustively
searched; no global-optimality claim is made.

The direct-context variant passed C simulation but did not finish the HLS flow
within a 600-second experimental limit. It is not adopted or claimed to save LUTs.
No arbitrary numerical narrowing, changed timing policy, truncated telemetry,
configuration-check removal, controller offload, or protection weakening is used.
Replacing exact arithmetic is outside this minimal-impact sweep.

Reproduction tools and invariants: [experiment guide](../experiments/lut_opt/README.md).
