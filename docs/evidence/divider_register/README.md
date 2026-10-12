# Combined dividend/quotient register

Baseline: `0ea8f180f06d64d65e5808d7eef5f84174105daf`.
The only functional source change is in `src/arithmetic.cpp`; clock, target and
HLS directives are unchanged. Baseline reports are retained here. Current kernel
reports are `../vitis_{motion,velocity,controller,dispatcher}_{csynth,cosim}.rpt`;
the current divider report is `combined_divide_csynth.rpt`.

Reproduce from the repository root:

```sh
cmake --build build -j4
python3 tools/check_motion_equivalence.py --reference 0ea8f180f06d64d65e5808d7eef5f84174105daf
python3 tools/check_dispatcher_equivalence.py --reference 0ea8f180f06d64d65e5808d7eef5f84174105daf
python3 tools/vitis_check.py --kernel dispatcher
python3 tools/vitis_check.py --kernel motion
python3 tools/vitis_check.py --kernel controller
python3 tools/vitis_check.py --kernel velocity
ctest --test-dir build --output-on-failure
```

Configure CMake with Icarus Verilog available to include RTL integration tests.
Native arithmetic tests use UBSan in the current build, exhaustive 8-bit inputs,
full-width boundary cases and 100,000 deterministic random pairs. Full motion
and dispatcher trace equivalence is recorded in `equivalence.log`.

Resource estimates and limitations are described in `../../verification.md`.
