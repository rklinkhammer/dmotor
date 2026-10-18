# 400 RPM shared velocity capability

> Current product builds: [build guide](build.md). Commands below are specialized
> component/experiment reproduction or historical evidence, not the default build.


The controller capability is now 400 RPM (400000 milliRPM), defined in
`include/dmotor/capabilities.hpp`. Each motor still has its own configured maximum;
this does not authorize a 130 RPM motor to run at 400 RPM. The JGA25-370 ratio is
10296/5 quadrature counts per output revolution; the provisional 37GB-520 ratio
is 1560/1. Their edge rates at 400 RPM are 13728 and 10400 counts/s respectively,
below the existing 100000 events/s capture limit.

## Arithmetic audit

| Quantity | Previous | Current |
|---|---:|---:|
| Signed velocity | 19 bits | 20 bits |
| Signed PI error | 20 bits | 21 bits |
| Maximum error magnitude | 400000 | 800000 milliRPM |
| Rounded P dividend, gain <= 1048576 | 419430400500 | 838860800500 |
| Shared exact one-phase PI divider | 39/20 bits | 40/20 bits |
| Rounded clamped I dividend | 65536500000 | unchanged |
| Clamped integral storage | 37 bits | unchanged |

The unclamped integral update uses wide intermediates: its absolute bound is
65536000000 + 1048576 * 800000 = 904396800000, before clamping and 37-bit narrowing.
The PI divisors remain 1000 and 1000000. The general 64-bit estimator divider,
64-bit position/event counters, 32-bit MMIO velocity fields, 20 MHz clock,
1 kHz control loop, and protection deadlines are unchanged.

The historical patches in `docs/evidence` remain immutable records. The preparation
pipeline applies `tools/apply_velocity_capability.py` after the exact-profile
patches; it connects optimized widths and limits to the shared capability header.
The historical q7/tenths experiments retain their old limits. Broad-range base
C++ sources remain an independent reference, not the optimized shipped profile.
The software configuration validator and semantic model use the same capability
header, which is also included in the deterministic Yocto source manifest.

## Reproduce

Use a fresh output directory; the preparation tools deliberately preserve old runs.

```sh
python3 tools/prepare_shared_mailbox.py --output build/rpm400_new
cmake -S build/rpm400_new -B build/rpm400_new/build
cmake --build build/rpm400_new/build -j4
ctest --test-dir build/rpm400_new/build \
  -R '^(arithmetic|motion|velocity|controller|dispatcher|mailbox|shared_mailbox)$' \
  --output-on-failure
python3 build/rpm400_new/tools/vitis_check.py --kernel dispatcher
python3 build/rpm400_new/tools/vitis_check.py --kernel motion
python3 build/rpm400_new/tools/vitis_check.py --kernel velocity
/opt/vitis/2026.1/Vivado/bin/vivado -mode batch \
  -source tools/measure_velocity_capability.tcl \
  -tclargs build/rpm400_new build/rpm400_new_routed
```

The isolated velocity-only tree retains legacy position/queue tests that do not
apply to its removed commands; the selected tests above exercise the retained
profile. Its generated mailbox documentation is not copied by the historical
preparation helper, so the full generated-tree CTest also reports that missing
artifact. Run the unmodified reference and software suites separately.

Vivado measurement routes the complete controller out of context on
xc7z020clg400-1 with a 50 ns clock and the HALT synchronizer exception. It measures
internal timing, not Arty pin timing, PS/SmartConnect integration, or board behavior.
The existing generated atomic-mailbox core is identical in both measurements.

To simulate or export a freshly generated engine explicitly:

```sh
DMOTOR_ENGINE_PROFILE=build/rpm400_new /opt/vitis/2026.1/Vivado/bin/vivado \
  -mode batch -source platforms/vivado/sim_controller_axi.tcl
DMOTOR_ENGINE_PROFILE=build/rpm400_new /opt/vitis/2026.1/Vivado/bin/vivado \
  -mode batch -source platforms/vivado/export_current.tcl
```

Exported controller revision 6 identifies this update. Existing packaged IP,
DVRMC bitstreams and EDF images must be rebuilt/upgraded before the new capability
is available on a board; updating host software alone is insufficient.

## Measurements and verification

HLS shared four-context engine, identical 20 MHz / xc7z020clg400-1 settings:

| Metric | 200 RPM baseline | 400 RPM | Change |
|---|---:|---:|---:|
| LUT | 25894 | 25909 | +15 |
| FF | 32416 | 32423 | +7 |
| DSP | 20 | 20 | 0 |
| BRAM18 | 0 | 0 | 0 |
| Maximum latency (clocks) | 333 | 336 | +3 |
| Maximum service interval (clocks) | 334 | 337 | +3 |

Baseline and candidate shared-engine C simulation, synthesis and RTL co-simulation
passed. Candidate motion and velocity kernels also passed all three HLS stages.
Motion tests cover exact +/-400000 targets, opposite-sign feedback through the
full ramp, maximum-gain saturation, independent unsaturated PI results, invalid
configuration/feedback rejection, and all 800001 accepted milliRPM conversions.
The arithmetic test checks the new 40-bit boundary, the maximum rounded PI
numerator, both production divisors, and 100000 randomized numerator/divisor pairs.
The estimator tests cover both ratios, both signs and event counts on either side
of the capability threshold. The shared core retains four independent contexts,
accepts boundary commands, rejects overspeed, and processes both encoder ratios.

Native: 18 reference checks, 7 selected optimized-profile checks and 3 software
checks passed with undefined-behavior sanitization. Software tests include
+/-400000 acceptance, +/-400001 rejection and a motor retaining its 200000 limit.
The complete AXI XSim test passed with both real HLS kernels. The generated-engine
Icarus transport test passed 2437 reference transactions, with a maximum observed
invocation latency of 318 clocks, including backpressure and timeout/reset checks.

The conservative six-services-per-motor frame estimate becomes
4 * 6 * 337 = 8088 clocks (404.4 us), before wrapper overhead, versus 400.8 us
previously. This remains below the unchanged 800 us frame deadline, but is not a
measurement of every possible integrated frame. Reports are retained under
[evidence/rpm400](evidence/rpm400/).

Complete controller Vivado results (paired out-of-context runs, completed
2026-10-15/16 America/New_York):

| Metric | 200 RPM | 400 RPM | Change |
|---|---:|---:|---:|
| Synthesized LUT | 34132 | 33997 | -135 |
| Routed LUT | 33830 | 33691 | -139 (-0.41%) |
| Routed FF | 50405 | 50402 | -3 |
| DSP48 | 16 | 16 | 0 |
| BRAM36 tiles | 2 | 2 | 0 |
| Setup WNS | +13.759 ns | +12.757 ns | -1.002 ns |
| Hold WHS | +0.062 ns | +0.039 ns | -0.023 ns |

Both runs passed setup/hold checks with zero failing endpoints and no DRC errors.
Warnings comprise DSP pipelining advice and the missing PS7 in the standalone
controller. OOC routing also warns that external ports have no partition pin
locations; these results do not qualify board I/O paths. The small LUT decrease
reflects synthesis/implementation optimization, not a general guarantee that wider
arithmetic saves resources. There is no material resource penalty in this run.

No board bitstream or EDF firmware was replaced by this measurement. The existing
`build/current_ip` export still contains the previous core until explicitly
re-exported with the freshly generated profile. Host capability changes must not
be interpreted as proof that an existing FPGA image supports 400 RPM.
