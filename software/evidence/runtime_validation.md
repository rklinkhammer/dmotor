# Lifecycle/runtime foundation validation — 2026-10-17

DMOTOR 0.6.0 source SHA-256:
`38ee912ca7e754b59efc7134cb4c4b9290c5f3939201aba33e4b5759bdaf0ecd`.
Working-tree implementation based on commit `8ee5114d90f7ee17999223f9fd0ede081f991baa`;
this is not a claim of a committed release.

- Native CMake build with UBSan: 5/5 CTest suites passed. The service test runs
  real service/menu processes with a test-only device backend, including killed
  client, lease expiry, disconnect, quit, EOF and partial-input HALT.
- EDF `bitbake dmotor-semantic-tests dmotor-access`: all 1214 tasks succeeded
  (1179 reused) on the final source snapshot. Four ARM user-mode QEMU executables
  passed, including lifecycle/velocity runtime tests.
- Kernel module compiled and packaged against Xilinx Linux 6.18.10-v2026.1;
  package QA passed. A separate compile against local Linux 7.0 headers passed.
- Generated kernel controller register header passed `generate_abi.py --check`.
- Production overlay generated from current EDF hardware compiled with `dtc -@`
  without warnings; the existing inspection overlay/input bundle was not replaced.
- No RTL changed; HLS/Vivado timing was not rerun for this software increment.
- No full SD image/export or physical driver/HALT/PWM/motor qualification was run.

Logs: [native](runtime_native_tests.txt), [ARM QEMU](runtime_arm_qemu.txt),
[target module](runtime_arm_module.txt). Final EDF build log:
`edf-dev/artifacts/logs/arty-z7-20/edf-dev-next-arty-z7-20-9170f0b75abe.log`.

The fake backend does not model all HLS/RTL timing, kernel removal, AXI aborts or
physical signals. Compilation and semantic tests do not qualify powered operation.
Continue with the drive-disconnected gates in [commissioning](../linux/production.md).
