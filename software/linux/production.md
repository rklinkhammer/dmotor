# Exclusive-access and velocity-runtime commissioning

Version 0.8.0 implements the first production-access/lifecycle foundation. It is
not yet qualified for powered motor operation. The target configuration selects inspection or production; the current EDF
checkout selects production commissioning. No service is started at boot and no driver is loaded by these tools.

## Access and HALT

The `dmotor_access` platform driver binds `dmotor,controller-0.18`, maps the
controller resource exclusively, provides `/dev/dmotor0` mode 0600, and permits
one open file owner. There is no mmap interface. UIO must not bind the same node.
Register ioctls are range-checked and serialized; `readl`/`writel` plus same-device
readback provide the kernel MMIO path. A completion read is not proof that every
FPGA command was accepted, so runtime configuration uses readback and lifecycle
operations check status/deadlines.

`halt-gpios = <&axi_gpio_3 0 0>` describes the saved DVRMC connection from
axi_gpio_3 output bit 0 to controller_axi_0/halt_async. Probe, open, close, removal
and explicit HALT assert that source. The driver checks GPIO readback and waits
up to 10 ms for disabled + blocked status. This is a host confirmation budget,
not the FPGA's 10 us digital inhibit requirement. CORE_RESET can itself produce
blocked status: physical HALT propagation must be measured after reset release.
Do not call that status check a complete electrical proof.

Open ownership is kernel-enforced; GPIO reservation prevents another GPIO
consumer from requesting this line. Overlay removal is vetoed while opened.
After platform removal, retained descriptors return ENODEV without MMIO access.
Administrative direct FPGA reprogramming, privileged /dev/mem and forced driver
unbind are outside this ownership boundary: prohibit them while attached.
A wedged AXI slave can still hang/abort a kernel access; this driver cannot turn
that into a guaranteed recoverable exception. FPGA watchdogs remain independent.
Reset/clock-loss electrical inhibition still belongs to the external adapter.

## Service and maintenance client

The standalone `dmotord` owns the device. It serves one client over a root-private
SOCK_SEQPACKET endpoint `/run/dmotor/control.sock`. A second client is refused.
Opening a service/client does not configure or enable motors. Explicit `r` performs
global inhibited reset, fresh nonzero session, complete configuration/readback,
watchdog/target timeout setup, GPIO release, ordered fault acknowledgments and
mailbox identity validation. Active fault sources prevent successful rearm.
Session/sequence exhaustion never wraps silently. Configuration is loaded from a
file at service startup; editing takes effect only after service restart/rearm.

Initial user-approved timing (2026-10-17): 100 ms authorization, 20 ms refresh,
500 ms client lease; 100 Hz target publication and 100 ms target timeout retained.
The service uses 5 ms absolute monotonic ticks, skipping missed ticks. Readiness
has a provisional 500 ms deadline. Configured per-motor STOP deadlines remain
independent. Measure all budgets under target load before claiming guarantees.

`dmotor-maint` is a separate interactive debug/maintenance application, not a test
harness. Its input event loop services the lease while waiting for input. It has
no independent heartbeat thread that conceals a blocked input loop. `!` requests
HALT immediately even during a partially typed command. Quit, EOF and handled
SIGINT/SIGTERM/SIGHUP request acknowledged all-motor HALT. Socket loss, malformed
messages, client crash or a 500 ms expired lease cause the service to HALT and
discard ownership. If the service dies, device close asserts HALT; if it stalls,
the enabled-motor FPGA watchdog remains the backstop. No reconnect resumes motion.
A failed confirmation is reported as unknown, not successful HALT.

The menu supports selection, status/position/velocity, configure/recover, enable,
atomic selected targets in signed RPM, controlled STOP, disable and HALT. The
runtime republishes the union of tracking motors, waiting for different pending
masks rather than partially replacing a batch. STOP and disable remove tracking
intent. STOP/re-enable requires global reset/rearm; peer-continuous rearm and
position/dwell planning are not provided. HALT on a rejected hardware command is
the initial conservative policy. Menu configuration editing and rich fault-history
views remain follow-up UI work; the validated configuration file is authoritative.

## EDF opt-in

Use `EDF_DMOTOR_ACCESS = "inspection"` for UIO bring-up. For the
production-access commissioning image, select `EDF_DMOTOR_ACCESS = "production"` in
`edf-dev/config/targets/arty-z7-20.conf`, then:

Use the software/image commands in EDF `docs/build.md`, with the same
`EDF_DMOTOR_SOURCE` used for the hardware bundle. Build instructions and bundle
selection are centralized there; the commands below are board runtime operations.


The firmware recipe changes only the controller binding and adds halt-gpios. The
image includes dmotor-runtime and dmotor-access, which pulls in the versioned kernel module. The matching
revision-6 FPGA already has the required HALT connection; no RTL change is needed.
Use a freshly booted image/full overlay, not a live UIO-to-driver rebinding.
After the usual FPGA Manager load, on the board with drive power disconnected:

```sh
sudo modprobe dmotor_access
sudo dmotord --externally-inhibited /dev/dmotor0 /usr/share/dmotor/inhibited-example.conf
# Separate serial/SSH terminal:
sudo dmotor-maint
```

The supplied example intentionally sets PI gains and duty limits to zero. It is
not a motor tuning profile. Inspection UIO tools are unavailable under this binding.

## Acceptance gates

1. Confirm driver probe, second-open rejection and correct HALT GPIO on a scope.
2. In the menu: r, e, s; readiness should appear without drive. Confirm fresh session
   and the expected encoder epoch reset. Test !, q, Ctrl-C, EOF, disconnect, stopped
   client (SIGSTOP), killed client and killed service, with external inhibition.
3. Confirm every qualified fault reaches all four output inhibits; measure delay.
   Confirm a new client cannot resume motion without explicit recovery/rearm.
4. With drive power still disconnected, use a reviewed nonzero-gain/duty profile
   and synthetic encoder inputs to inspect PWM frequency, duty, direction, STOP,
   target-age and watchdog expiration. Keep sensor-loss/acquisition limits active.
5. Only after those checks and electrical adapter/current-limit review, begin
   bounded single-motor powered testing, then multiple motors and loaded tests.

Native tests cover lifecycle/error behavior and real service/client processes with
a test-only device backend. ARM user-mode QEMU covers runtime/protocol tests.
Neither executes the kernel driver against real PL. Kernel compilation and source
review do not establish board removal safety, MMIO latency or electrical response.

Validation of the initial implementation: [runtime evidence](../evidence/runtime_validation.md).

## Next commissioning gate

Follow [drive-disconnected output validation](output-validation.md) for the bounded
nonzero-duty profile, opt-in timeout injection (`x`/`w`) and scope acceptance record.
The maintenance client remains a manual debug application; no powered mode is provided.

Direct PWM without encoder feedback is available on the new DMI2 controller; see
[direct-duty commissioning](direct-duty.md). It requires a matching rebuilt FPGA
and driver, and does not authorize powered operation.
