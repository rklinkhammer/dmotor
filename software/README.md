# DMOTOR processor software — first increment

This directory starts Phase 0 of [DMOTOR_SW.md](../DMOTOR_SW.md). It builds
independently of Vitis/Vivado and the legacy controller code. It currently contains
an **in-process semantic model, typed protocol, inhibited Linux inspection tool
and automated tests**, not a deployable motion service or maintenance application.

## Native validation

```sh
cmake -S software -B build/software -DDMOTOR_SW_SANITIZE=ON
cmake --build build/software -j4
ctest --test-dir build/software --output-on-failure
```

Tests retain their checks in Release builds. The model uses the real
`AtomicMailbox` and `publish_velocity_batch` helper, including ABI identification,
full staging, commit, same-mask replacement, replay rejection and atomic retirement.
It adds explicit virtual control ticks, typed admission limits and snapshots,
authorization/target timeouts, exclusive ownership and maintenance lease loss.

The tests cover multi-motor atomic application, mask conflicts, stale sequence and
configuration rejection, signed velocity range, expiration winning simultaneous
refresh/application, READY authorization, zero-target freshness, normal maintenance
close, lease renewal/expiry, stale ownership tokens and explicit rearm without replay.

## ARM validation through EDF and BitBake QEMU

From the dmotor checkout, stage the explicit source subset and recipe:

```sh
python3 software/yocto/stage.py ../edf-dev
cd ../edf-dev
./edf --target zc702 bitbake dmotor-semantic-tests
# Force another execution even if the build is already up to date:
./edf --target zc702 bitbake -c semantics -f dmotor-semantic-tests
```

Restage after modifying the model or tests. The staging script updates only
`meta-edf-dev/recipes-dmotor/dmotor-semantic-tests/`. The recipe uses `LICENSE =
"CLOSED"` because this repository currently supplies no distribution license;
this does not establish a new project license.

BitBake cross-compiles for EDF's Cortex-A9 hard-float Linux target and executes
`dmotor-semantic-tests` using **qemu-native user-mode emulation** with the target
sysroot. `do_semantics` runs between compile and install, fails the build on test
failure, and records output in the recipe's `temp/log.do_semantics`. No host ARM
compiler or separately installed QEMU is needed. The tests are packaged as
`/usr/bin/dmotor-semantic-tests`; they are not automatically added to the production
image or launched at boot. A future full-system EDF QEMU image can install this
package and execute the same tests in its guest. ZC702 supplies the ARM build
context here; this is not validation of Arty's board peripherals or custom PL.

## Model boundaries

- Timeouts are explicit test inputs in 1 ms control ticks. The initial 100-tick
  target-age constant represents the architecture's 100 ms proposal. Test values
  do not select the production authorization or maintenance-lease budgets.
- The model clock is monotonic across rearm; it is a service/test clock, not the
  FPGA encoder/time epoch that CORE_RESET resets.
- The model executes all selected computations successfully within one tick.
  It does not simulate HLS latency, warm-up, fault acknowledgment/reset sequencing,
  AXI transactions, MMIO ordering, CDC, PWM, encoder signals or motor mechanics.
- `MotorLimits` is only the mailbox admission subset, not the complete hardware
  configuration. Snapshots contain requested velocity, not invented measured speed
  or position. STOP/ramping, numerical configuration validation and full telemetry
  decoding remain unimplemented.
- Rearm is an explicit semantic operation requiring a new session. It does not
  claim that writing CORE_RESET clears physical protective latches. Real recovery
  must implement the documented hardware acknowledgment and external-HALT checks.
- HALT clears modeled authorization/intent and latches a fault. It is an abstract
  shared-HALT action, **not a nonexistent HALT MMIO register**. The production
  backend still needs the qualified GPIO/shared-HALT binding and acknowledgment.
- The model is single-threaded. Owner tokens represent service-level ownership,
  not OS access controls. A virtual lease expires only when tests advance time;
  independent hardware watchdogs must remain effective if the real service dies.
- `MaintenanceSession` tests orderly scope exit. Crash behavior is tested through
  separate lease expiry without invoking that destructor. The future service must
  also monitor disconnects and enforce exclusive ownership independently of the UI.

`dmotor-maint` remains a separate debug/maintenance application, never an automated
test harness. Its deployed version will use the sole-owner service, offer no mock
motion mode, and HALT all motors on exit or ownership loss. No powered operation
is enabled by this increment.

The [typed protocol](protocol.md) now provides full configuration serialization,
status/telemetry decoding and an explicit backend interface. Next: implement the
production backend and sole-owner service. The opt-in [Linux inspection backend](linux/README.md)
is now implemented for inhibited bring-up, with advisory ownership only. Bind and test
shared HALT before enabling maintenance motion; add the separate terminal UI using
that service rather than giving it direct MMIO access.

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
qualification](linux/README.md). The separate maintenance application is unchanged
in scope and still awaits the sole-owner service and HALT lifecycle.
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
DMI1, so software cannot read back proof of its assertion; position changes and
board output measurements remain hardware acceptance checks.

### Version 0.6.0 lifecycle/velocity foundation

See [production-access commissioning](linux/production.md) for the exclusive
kernel driver, `dmotord`, separate `dmotor-maint` menu and opt-in EDF binding.
These additions require inhibited board qualification before powered operation.
The existing UIO/encoder workflow remains the default image configuration.
