> **Historical record — not current authority.** Snapshot from `20f5972`.
> [Byte-exact original](ps_monitor_progress.original.txt); current status is in the [system status](../../../system_status.md).

# PS monitor development — transport increment, 2026-11-03

2026-11-04: the prior hardware-bundle blocker is resolved. The full EDF build,
ARM kernel/driver build, QEMU semantics and SD export passed. See
[current build evidence](current_build_2026_11_04.md). Recovery/ARM development
remains incomplete as described below.

Implemented the first PS monitor protocol increment. This is not a completed
recovery coordinator and does not enable the monitor's physical ARM sequence.
The existing commissioning daemon is unchanged in that respect.

## Shared protocol and serial I/O

- [Wire schema](../../../../software/protocol/monitor/wire.json) defines version-1 message
  IDs, payload layouts, states and NAK reasons. Its generator owns shared C++
  constants/lengths; `generate.py --check` detects drift. Cause-bit allocation
  remains reserved for the monitor safety-core implementation.
- Fixed-storage codec implements little-endian fields, big-endian CCITT-FALSE
  CRC, delimiter/escape handling and resynchronization. It rejects invalid
  versions/types/lengths, malformed escapes, corruption and oversized frames.
- Linux UART transport uses exclusive raw 115200/8N1, nonblocking I/O, one TX
  slot with a deadline and bounded RX work per poll. It retains fragmented
  frames and does not discard adjacent frames. The caller supplies a monotonic
  clock and is responsible for recovery hold/HALT on transport failure.
- [Protocol notes](../../../../software/protocol/monitor/README.md) document its limits.
  Parsing a frame does not establish peer freshness or authorize any action.
  Session pairing, duplicate detection and recovery retries are still required.

## Kernel and Yocto integration

`dmotor_access` requires HB11 at probe while HALT remains asserted. MONITOR_ACK
checks disabled state, healthy observation, core reset and nonzero PL session
before writing. Hardware still resolves races and the runtime must verify ACK
completion. GPIO HALT release now checks readback and reasserts on failure.
No ioctl numbers changed. Use this driver only with the matching monitor-capable
bitstream; it deliberately refuses earlier controllers.

In companion `edf-dev`, the Arty kernel fragment now enables built-in UART Lite
and disables its console. A configure check rejects silently dropped settings.
The firmware recipe validates the generated UART node at 0x42c00000, its 64 KiB
window, Linux compatible, 115200 baud and IRQ properties. It rejects missing or
disabled nodes rather than patching old hardware descriptions. Generated clocks
and IRQ assignments remain owned by the matched hardware export. Do not assign
a getty to the monitor tty. The existing PS UART remains the console.

Yocto source staging includes the codec, serial implementation and tests;
`do_semantics` also runs the portable monitor codec test under target QEMU.
The pseudo-terminal test is available for native Linux and installed for target
Linux testing, but is not a UART Lite electrical/FIFO test.

## Validation and original build limitation

- Native UBSan build: all 11 CTest suites pass, including codec, PTY and generated
  schema checks, plus the existing runtime/commissioning tests.
- Updated kernel module compiles against local Linux 7.0.0-42 headers. BTF was
  skipped without vmlinux; this does not establish ARM target compatibility.
- EDF validator tests cover accepted UART topology and reject stale/missing,
  wrong-baud/window, missing-IRQ, disabled and console-bound UART descriptions.
- Attempted `EDF_DMOTOR_SOURCE=../dmotor ./edf --target arty-z7-20 bitbake
  dmotor-semantic-tests dmotor-access`. EDF refused before BitBake because the
  installed hardware bundle differs from current PL sources. The old export
  also lacks UART Lite. No fingerprint check was bypassed and no image/ARM build
  success was claimed in that first attempt. This limitation was subsequently
  resolved by the successful full build linked above.

## Next increment

Implement persistent endpoint sessions, pairing and transaction freshness, then
the PS recovery coordinator and runtime admission hold against a simulated
monitor. Verify PL cleanup before constructing attestation. Add the ESP32
endpoint and resolve cause bits; test lost COMMIT_ACK, restart, new epochs and
physical ARM races together. The contract remains PROPOSED v4 until those
behaviors and target measurements are qualified. Nonblocking daemon diagnostics,
vehicle planning and PL clock-loss supervision remain separate open work.
