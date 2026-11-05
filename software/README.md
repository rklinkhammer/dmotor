# DMOTOR software — Zynq runtime and planned ESP32 firmware

Build and cross-compilation instructions: [component guide](../docs/build.md) and
EDF `docs/build.md`. This document describes software interfaces and board use.


This directory contains the Zynq semantic model, typed protocol, Linux access,
runtime, maintenance/commissioning tools and native tests described in
[DMOTOR_SW.md](../DMOTOR_SW.md). Its native build is independent of Vitis/Vivado.
The **ESP32 firmware will be included here under [esp32/](esp32/README.md)**,
with a separate ESP-IDF build and host-testable C core. That implementation is
pending; the existing build and historical evidence below concern Zynq software.

## Build and validation

Use the [component build guide](../docs/build.md) for native validation and EDF
`docs/build.md` for ARM/BitBake QEMU and image builds. EDF stages the selected source
automatically; do not manually copy or stage a different checkout. User-mode QEMU
validates software semantics, not the FPGA or kernel driver's physical behavior.

## Current interfaces and implementation

Start with the [controller programming guide](../docs/controller_programming.md)
for service IPC, runtime APIs, kernel ioctls, PL configuration/telemetry, timing,
ownership and monitor UART rules. See [DEVELOPMENT.md](DEVELOPMENT.md) for the
clang-format/clang-tidy, editor and native-test workflow.

The exclusive kernel driver, `dmotord`, `dmotor-maint`, typed protocol and runtime
are implemented as a commissioning foundation. The monitor codec/UART library is
implemented but is not connected to the daemon's recovery lifecycle. Joint monitor
recovery and physical ARM admission remain pending. See
[production access](linux/production.md) and [PS monitor progress](../docs/ps_monitor_progress.md).

The semantic model remains a single-threaded reference/test model, not a simulation
of HLS latency, AXI/CDC, electrical signals or mechanics. Fake-device and process
tests do not qualify hardware behavior. UIO inspection/encoder tools use a separate
inhibited binding with cooperative ownership only.

## Recorded validation

Native UBSan tests and EDF Cortex-A9 QEMU execution passed on 2026-10-14;
BitBake package QA also passed. See [evidence](evidence/validation.txt).

The next increment adds `dmotor-protocol-tests` to CTest and the same BitBake
QEMU task. See [protocol details and transport contract](protocol.md).
Both suites passed natively with UBSan and under EDF ARM QEMU; package QA passed.
See [protocol validation](evidence/protocol_validation.txt).


## Linux inspection increment

`dmotor-linux-tests` joins native CTest and EDF ARM QEMU validation. The recipe
produces a separate opt-in `dmotor-inspect` package; it is never added to the
production image or started automatically. See [Linux integration and remaining
qualification](linux/README.md). This is historical validation of the inspection increment; the current service
and HALT lifecycle are described in the programming guide.
Native UBSan and EDF ARM QEMU passed all three suites; the separate inspection
package passed QA. See [Linux validation](evidence/linux_validation.txt).

## Source identity

Every executable prints version, Git commit, dirty state, source SHA-256 and ABI at
startup; `--version` exits before tests or hardware access. The same identity is
installed at `/usr/share/dmotor/source-version.json`. The fingerprint covers the
explicit `software/sources.json` list, including build/packaging inputs. Build-time
provenance is regenerated on each native build; staged builds verify their source
hash against preserved metadata. No compilation timestamp is used.

EDF now normally stages automatically from its pinned `external/dmotor` submodule.
Manual sibling staging is a development workflow, not the release source of truth.

### Encoder-only board bring-up

`dmotor-encoder` is a separate maintenance utility, never started by the test
suite or a boot service. With motor drive power disconnected, encoder power and
FPGA-safe A/B levels verified, run:

```sh
sudo dmotor-encoder --externally-inhibited 0
```

The controller must be disabled and unconfigured (fresh FPGA load). The utility
holds the cooperative UIO lock, establishes/reuses a nonzero session and releases
core reset so the quadrature decoders can run. It prints all four raw positions
at 250 ms intervals. Rotate one shaft by hand: only its channel should change;
reverse rotation should reverse the count direction. For the provisional 13
pulse/channel, 30:1 motor expect 1560 counts per output revolution in 4x mode.
Confirm the actual encoder ratio by measurement. RPM/valid telemetry is not a
criterion here because the control engine is not configured or enabled.

Ctrl-C/SIGTERM and normal exception unwinding restore core reset (position is
reset). No configuration, enable, target, watchdog refresh or fault acknowledgment
writes are allowed. SIGKILL, process crash, FPGA removal or AXI bus abort can
prevent cleanup; external drive inhibition must remain in place throughout.
Do not reload/unbind the FPGA while this utility is running. The lock is
cooperative and cannot exclude unrelated raw MMIO users. Reset is write-only in
DMI2, so software cannot read back proof of its assertion; position changes and
board output measurements remain hardware acceptance checks.

### Version 0.6.0 lifecycle/velocity foundation

See [production-access commissioning](linux/production.md) for the exclusive
kernel driver, `dmotord`, separate `dmotor-maint` menu and opt-in EDF binding.
These additions require inhibited board qualification before powered operation.
UIO/encoder and production commissioning use distinct image bindings; the current
EDF configuration selects production commissioning.

### Software 0.8.0: direct PWM commissioning

[DMI2 direct-duty procedure](linux/direct-duty.md) adds an adjustable signed-duty
ceiling and optional measured floor without encoder stimulus. Requires matched new
FPGA/driver/software. The 0.7 velocity validation procedure remains separate.

Software 0.9.0: [bounded powered commissioning and next board test plan](linux/powered-commissioning.md).
