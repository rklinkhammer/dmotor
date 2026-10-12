# One-phase divider applied to the active full profile

Baseline source: `4d547eb58b48c5aec98cda173c1a5adc40562358`.
The change is in `src/arithmetic.cpp` and its interface comment. It keeps the
unsigned 64-bit API, all input ranges, signed caller rounding, and full motion
features. Each of 64 iterations now compares and conditionally subtracts in one
phase, preserving the remainder carry. No velocity-only restrictions are adopted.

Toolchain: Vitis 2026.1, xc7z020clg400-1, 20 MHz. Reports are HLS estimates, not
post-route results. Prefix `baseline_` denotes preserved pre-change reports;
unprefixed reports denote the new implementation.

| Kernel | LUT before / after | FF before / after | DSP | BRAM18K | Max cycles before / after | New max us | New path ns |
|---|---:|---:|---:|---:|---:|---:|---:|
| Motion | 16616 / 16193 | 7135 / 7129 | 32 | 0 | 752 / 432 | 21.60 | 32.271 |
| Velocity | 3673 / 3532 | 1878 / 1876 | 7 | 0 | 132 / 68 | 3.40 | 23.369 |
| Controller | 23793 / 23370 | 10526 / 10520 | 39 | 0 | 951 / 567 | 28.35 | 36.161 |
| Dispatcher | 24578 / 24155 | 12107 / 12101 | 39 | 4 | 1064 / 680 | 34.00 | 34.572 |

The full 22-test suite passes. Motion trace comparison against the baseline
matches 16387 snapshots; dispatcher comparison matches 1379 complete replies.
Tests preserve full-width division boundary coverage and 100000 random pairs.
The four affected HLS kernels are motion, velocity, controller and dispatcher;
other kernels are not regenerated because they do not use this divider.

Reproduce native tests with the normal CMake/CTest commands. Reproduce HLS using
`python3 tools/vitis_check.py --kernel KERNEL` for the four affected kernels.
Equivalence tools accept `--reference` with the baseline revision above.

Historical bounded-profile experiments were generated before this main-source
change. For exact historical reproduction, use their recorded revision and patch
chain; freshly preparing profiles from the updated main source also inherits the
one-phase general divider, and can change their resource measurements.

Motion, velocity, controller and dispatcher all passed Vitis C simulation,
synthesis and Verilog co-simulation. Their updated reports have also replaced
the current `docs/evidence/vitis_*` reports.
