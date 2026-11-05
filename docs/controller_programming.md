# Controller programming guide — Zynq PS

This is the consolidated programming reference for the **implemented** PS
commissioning software. It covers interfaces, ownership, sequencing, units and
failure handling. Use [build.md](build.md) for hardware/EDF builds and
[PS development](../software/DEVELOPMENT.md) for coding and validation commands.

The controller driver, service, runtime, typed PL protocol and monitor framing/
UART transport exist. **Monitor pairing, persistent endpoint sessions, the joint
recovery coordinator and physical ARM admission are not implemented.** The daemon
currently performs PL preparation only. It is not the complete production safety
implementation of the [proposed recovery contract](recovery_transition_contract.md).
The ESP32 implementation belongs in `software/esp32/` in this repository.

## 1. Layers and ownership

| Interface | Owner and intended caller | Source of truth |
|---|---|---|
| Application → client library → daemon | One local client; daemon owns all motion intent | Public `software/include/dmotor_sw/client.hpp`; internal `service_protocol.hpp` |
| Runtime lifecycle API | One serialized executor in `dmotord` | `software/include/dmotor_sw/runtime.hpp` |
| Typed configuration/status/telemetry | Runtime → `Protocol` → `Device` | `software/include/dmotor_sw/protocol.hpp` |
| Platform timing/HALT/MMIO | `KernelDevice` implements `Platform` | `software/linux/kernel_device.cpp` |
| Userspace → kernel | Exclusive `/dev/dmotor0`, ioctl only | `software/linux/driver/dmotor_ioctl.h` |
| PS → PL register/atomic target interface | Kernel orders and restricts accesses; PL validates commands | `abi/controller.json`, generated `include/dmotor/controller_registers.hpp`, `include/dmotor/batch_registers.hpp` |
| PS → monitor UART | Dedicated owner of `monitor::Uart`; not yet used by daemon | `software/protocol/monitor/wire.json` |
| Inhibited diagnostic UIO | Separate inspection/encoder workflow; cannot coexist with driver binding | `software/linux/README.md` |

All applications communicating with the daemon must use the
[`dmotor_client` library](../software/CLIENT.md), exposed as `dmotor::client` and
`dmotor_sw::Client`. `dmotor-maint` uses it; new clients must not duplicate socket
serialization or bypass the daemon through the driver. Extend `Runtime` for lifecycle
behavior and `Protocol` for typed serialization; do not add raw register writes to
clients. `Device` requires aligned, peripheral-relative 32-bit reads/writes and a
completion fence. Access failure throws `IoError`; never substitute zero or stale
data. `Platform` adds confirmed HALT, monotonic microseconds and bounded sleep.
These APIs are serialized, not thread-safe shared objects.

`src/` at repository root contains controller/HLS algorithms and shared helpers;
`software/src/` contains PS userspace implementation. `rtl/` owns handwritten PL.
Changing PS code does not itself require a new bitstream; changing the register
contract or PL implementation requires matched hardware, generated headers and
software. Check capability IDs, not just a software version string.

## 2. Local client protocol

The daemon listens on `/run/dmotor/control.sock`, AF_UNIX/SOCK_SEQPACKET, in a
0700 directory with a 0600 socket. Only one client is admitted. Connect/open does
not prepare or enable anything. There is no TCP or remote-control protocol.

The following wire details are for protocol maintenance and testing. Applications
call typed Client methods; the internal wire header is not an application SDK.

Schema 1 uses the native local C++ ABI with fixed-width fields and size assertions:
`Request` is 28 bytes; `Response` is 192 bytes. It is **not** a portable serialized
network ABI. Match the header and host byte order; on the current ARM/Linux target
32-bit words are little-endian. Send exactly one Request per packet, then read one
complete Response. Short/oversized requests or wrong schema disconnect and HALT.

Request fields: `schema`, `operation`, `mask`, and `int32_t velocity[4]`.
Motor index is 0–3; mask bit `1U << motor` selects that channel. Valid nonempty
masks are 1–15; individual operations further constrain configured/enabled motors.
Initialize unused fields to zero.

| Operation (schema-1 number) | Input and effect |
|---|---|
| ping (0) | Renew client lease; no motion change |
| status (1) | Renew lease and capture measured velocity/position |
| prepare (2) | Global PL preparation using daemon's startup configuration; ignores request mask |
| enable (3) | Enable selected prepared motors; readiness is asynchronous |
| velocity (4) | Set signed milliRPM for selected ready motors; runtime publishes later |
| stop (5) | Controlled STOP for selected enabled motors; clears their tracking intent |
| disable (6) | Disable selected channels and discard their tracking intent |
| halt (7) | Global HALT and discard all intent |
| quit (8) | Global HALT, reply, then disconnect |
| inject_target (9), inject_watchdog (10) | Inhibited commissioning tests only |
| duty (11) | `velocity[0]` carries signed Q16 duty, not milliRPM; requires explicit single-motor direct-duty policy |

Response fields: `schema`, `okay`, raw `status`, global `applied` sequence,
`measured[4]`, `position_lo[4]`, `position_hi[4]`, and `message[128]`.
Measured velocities and positions are populated for **status requests only**;
zeros in other replies are not observations. Reconstruct position as a signed
64-bit two's-complement value from high/low words. The service response does not
expose all telemetry validity/fault fields; extend the versioned API if needed.

`okay=1` means the handler completed, not that motion has physically occurred or a
new target has already applied. Track the global applied sequence separately.
`okay=0` reports a rejected command and the daemon attempts HALT. The client
library returns this as `ClientResult::okay=false`; transport/protocol failures
throw `ClientError` and permanently close that connection. A disconnect or
missing response leaves outcome unknown; reconnect never resumes motion. Use typed client methods rather than embedding operation numbers or constructing
wire requests in applications.

The maintenance client sends a ping every 100 ms while idle. Current policy permits
500 ms without client activity. All structurally valid requests renew the lease;
a ping does not itself clear faults, refresh motion intent or authorize recovery.

## 3. Runtime lifecycle and ordering

`Runtime` construction asserts HALT; destruction attempts HALT but cannot report
cleanup success. The service explicitly confirms HALT at orderly shutdown.

`prepare(configurations, mask)` currently performs this global inhibited sequence:

1. Validate the selected configurations, HALT and verify disabled status.
2. Check DMI2, establish a new nonzero PL session without wrapping, and assert core
   reset. Reset the previous session before replacing it when one already exists.
3. Serialize/commit all 20 configuration words per selected motor and verify
   readback. Configure/verify direct-duty mode if explicitly selected.
4. Set watchdog and target-age budgets. Require HB11, wait for healthy monitor HB
   while disabled/reset, write MONITOR_ACK and verify the monitor fault cleared.
5. Release the PS-owned HALT GPIO, issue ordered ACKNOWLEDGE values 1, 2, 4 with
   settling delays, release core reset, and verify faults/HALT/blocked cleared.
6. Require qualified monitor status, validate DMB1 for velocity mode, discard old
   software intent/sequences and start a new client lease.

Failure attempts HALT and propagates the error. Preparation affects **all motors**;
it is not peer-continuous recovery. `prepare()` neither enables channels nor
completes a monitor UART/physical ARM exchange.

`enable(mask)` begins readiness acquisition. Wait for all selected ready bits
before `velocity()` or `duty()`; configured, enabled and ready are distinct states.
`velocity()` updates software intent; `tick()` publishes the union of tracking
motors as one atomic batch. A changed pending mask must retire before another
mask replaces it. A published sequence is not proof of application: use the
coherent applied-batch snapshot, not per-engine telemetry sequence fields.

STOP removes selected intent and follows the PL deceleration/settling path in
velocity mode. Direct-duty STOP disables that channel. Disable also removes intent.
The current runtime requires global prepare before a stopped/disabled channel can
be enabled again. Global HALT asserts the owned GPIO and clears all software intent.
Do not implement controlled STOP by asserting global `coast_req`. Per-channel
disable is not a promise of electrical coast; see [controller AXI](controller_axi.md)
and [recovery contract](recovery_transition_contract.md) for braking/coast semantics.

Sequence and session exhaustion fail closed; never wrap to zero. A lifecycle error
or partial transaction must be reconciled while inhibited, not blindly retried.
Standalone users of Runtime must apply the daemon's HALT-on-error policy too:
not every public API method catches transport errors internally.

## 4. Timing, configuration and numeric rules

| Quantity | Current default / representation |
|---|---|
| Service tick | 5 ms, absolute monotonic schedule, missed ticks skipped |
| Target publication | 10 ms (100 Hz) |
| Authorization refresh / expiry | 20 ms / 100 ms |
| Target-age timeout | 100 ms, independent of authorization |
| Client lease | 500 ms |
| Readiness wait | 500 ms |
| Monitor health preparation wait | 1.5 s |
| Runtime transition wait / poll | 20 ms / 100 µs |
| Driver HALT confirmation budget | 10 ms; not a measured electrical latency |
| Powered direct-duty pulse | At most 1 s; updates cannot extend it; explicit rearm required |
| Velocity / acceleration | Signed milliRPM / milliRPM per second |
| Duty and gains | Q16; duty magnitude 65536 means 100% |
| Fabric timing | 20 MHz controller cycles, 20 cycles per microsecond |
| Encoder position | Signed 64-bit counts, not degrees or revolutions |

Use named constants in `RuntimePolicy`, `dmotor/timing.hpp` and the service header.
Monotonic host time, PL sample ticks and monitor epochs are different clocks; do not
compare them directly. Linux scheduling and device access require target-load
measurement. Existing synchronous diagnostic output can block the executor; this
remains work before claiming bounded production service timing.

The daemon reads its text configuration once at startup. Rows have exactly:

```text
motor generation maximum_mrpm acceleration_mrpm_s deceleration_mrpm_s
counts_numerator counts_denominator position_tolerance_counts velocity_tolerance_mrpm
settle_ticks kp_q16 ki_q16 duty_limit_q16 integral_limit_q16 stop_timeout_us
zero_timeout_cycles acquisition_timeout_cycles acquisition_duty_limit_q16
```

These are **18 fields on one line**, shown wrapped above for readability. Motor
rows must be unique. A comment starts with `#` in column one; empty lines are
ignored. Extra fields, negative text and invalid configurations are rejected.
Use `valid_configuration()` rather than duplicating its bounds. The software speed
profile currently admits 1–400 RPM; actual per-motor maximum, CPR rational, gains,
limits and timeouts must be commissioned for the hardware. Reserved serialized
words are zero. PL validation/readiness remains authoritative. Position tolerance
in the layout does not imply support for position or dwell commands.

Use fixed-width integers at interfaces, explicit unit suffixes and widened
intermediates before multiplying, subtracting or negating. Check ranges before
narrowing. No floating-point values or pointer-bearing structures cross the wire.
Preserve sign and units when converting terminal RPM/percent inputs.

## 5. Kernel and PL boundary

`dmotor_access` binds `dmotor,controller-0.18`, reserves the resource and HALT GPIO,
provides exclusive open of `/dev/dmotor0`, and exposes no mmap interface.
`DMOTOR_READ`/`DMOTOR_WRITE` carry `{uint32 offset, uint32 value}`;
`DMOTOR_HALT` asserts the owned source and `DMOTOR_RELEASE_HALT` deasserts it.
Release does not acknowledge protective latches or grant permission to move.
The driver checks register allowlists and command preconditions; reads/writes use
kernel MMIO primitives and readback. FPGA acceptance still needs explicit status
or sequence confirmation. A wedged AXI slave is not guaranteed recoverable by ioctl.

Important register groups (all byte offsets relative to the controller):

| Group | Location / rule |
|---|---|
| DMB1 atomic mailbox | 0x000 onward; inaccessible during blocking reset/HALT; see [mailbox map](batch_registers.md) |
| DMI2 local identity/status/session | 0x1000 / 0x1004 / 0x1008; local identity is readable while blocked |
| Lifecycle | ENABLE 0x100c, DISABLE 0x1010, STOP 0x1014, CORE_RESET 0x1020, ACKNOWLEDGE 0x1024 |
| Authorization / age | REFRESH_SESSION 0x1018, watchdog 0x1028, target timeout 0x102c, per-motor refresh 0x1040 + 4×motor |
| Snapshot | 0x1030; capture bank uses sequence validation |
| Direct duty | 0x1050–0x1060; capability DDU1, explicit commissioning policy |
| Monitor | HB11 0x1064, status 0x1068, ACK 0x106c |
| Configuration | 0x2000 + 0x100×motor; commit at channel base + 0x80 |
| Telemetry | 0x3000 + 84×motor; 21 words per motor |

STATUS: enabled bits 0–3, configured 4–7, ready 8–11, stopped 12–15, fault 16,
shared HALT 17, blocked 18, target-age faults 19–22. MONITOR_STATUS: healthy bit 0,
fault bit 1, qualified bit 2, core-reset bit 3. A healthy heartbeat alone is not
motion qualification. ACK=1 requires disabled/core-reset, nonzero session and
healthy HB; verify resulting status because a simultaneous expiry can win.

Do not hand-edit generated register headers; change the ABI source and generator
outputs together. The [full AXI reference](controller_axi.md) and [atomic mailbox
rules](atomic_mailbox.md) specify individual register side effects.

`Protocol::configure()` requires core reset already asserted and no enabled motor;
it does not orchestrate reset or recovery. `Protocol::capture()` makes one bounded
coherent-bank attempt: false means concurrent capture, `IoError` means transport
failure, and both leave the caller's snapshot unchanged. `Runtime::capture()`
turns an incoherent attempt into an error. Telemetry validity/acquisition/fault bits
must accompany numeric values. Snapshot coherence does not mean all fields were
sampled at one physical instant: engine and encoder/protection observations differ.
Do not treat requested velocity as measured speed or as reliable target evidence
after an engine/adapter fault.

Driver probe/open/close/removal asserts HALT; active ownership vetoes normal overlay
removal. Forced unbind, privileged raw MMIO or direct FPGA reprogramming bypass
these guarantees. Do not perform them while attached. UIO inspection is a separate
inhibited diagnostic binding with only cooperative locking, not an alternate
production motion API.

## 6. Monitor transport and recovery boundary

UART Lite is at 0x42c00000, raw 115200/8N1 without flow control; select the tty from
the matched device tree/sysfs, never assume a tty index. Do not attach a console or
getty. `monitor::Uart` has exclusive ownership, nonblocking I/O, one fixed TX slot,
an absolute monotonic TX deadline and bounded RX work per poll.

Wire v1 uses explicit little-endian fields, big-endian CRC-16/CCITT-FALSE, 0x7e
framing and 0x7d escaping. Maximum decoded/encoded frame sizes are 97/196 bytes.
Sessions, peer echo, epoch, transaction and sequence belong to the versioned
[schema and transport specification](../software/protocol/monitor/README.md).
Never send a packed native structure over this UART. Completing a write means
kernel acceptance, not peer acknowledgment.

The current decoder validates framing and field shape only. It does not establish
freshness, session pairing or recovery authority. The proposed recovery protocol
requires verified PL cleanup, session/epoch/transaction binding, COMMIT_ACK-loss
reconciliation and physical ARM admission. Until that coordinator and the ESP32
endpoint exist, a valid UART frame or healthy HB must not be advertised as proof
that the joint recovery contract has completed.

## 7. Change and validation rules

- Keep clients free of MMIO, watchdog-refresh and GPIO ownership. Keep slow UI,
  persistence and diagnostics out of the executor's critical path.
- Treat failed completion/acknowledgment as an unknown outcome; inhibit and report
  it. Never auto-resume on reconnect, healthy HB or cleared fault status.
- Update interface definitions, implementation, adverse-path tests and this guide
  together. A new socket layout requires schema/version handling; a monitor layout
  change starts in `wire.json`; a PL register change starts in `abi/controller.json`.
- Run PS formatting, static analysis, UBSan and CTest as described in
  [DEVELOPMENT.md](../software/DEVELOPMENT.md). Use fake-device tests for software
  faults, ARM/Yocto validation for deployed code, and scope/board tests for physical
  behavior. None substitutes for the others.
- Preserve source identity (`--version`, `software/sources.json`, installed
  `source-version.json`) and matched PL/driver/runtime identities. Source edits
  require a new software package/image to update deployed evidence.

For actual board operations, follow [production-access commissioning](../software/linux/production.md),
[output validation](../software/linux/output-validation.md) and the separately gated
[powered commissioning](../software/linux/powered-commissioning.md) procedures.
