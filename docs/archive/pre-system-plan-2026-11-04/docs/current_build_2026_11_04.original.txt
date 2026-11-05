# Current DMOTOR / EDF build — 2026-11-04

The full EDF product workflow completed using the current development checkout:

```sh
cd ~/workspace/edf-dev
EDF_DMOTOR_SOURCE=../dmotor ./edf --target arty-z7-20 build
```

This supersedes the previous mismatched-hardware blocker. It updates build
artifacts and EDF's installed hardware inputs; it does not implement the remaining
recovery/ARM coordinator or ESP32 firmware. No board was programmed or SD card
written. Physical operation remains unqualified.

## Hardware and software included

- Controller revision 10; user-confirmed mon_hb=CK_IO2/U13 and
  coast_req=CK_IO3/V13. UART remains on CK_IO0/1.
- Restored explicit UART Lite 115200 baud, 8 data bits, no parity. Project creation
  now runs the source interface checker so a future saved-BD regression fails
  before synthesis. The generated SDT and installed overlay confirm 0x42c00000,
  64 KiB, 115200 baud, clock and interrupt wiring.
- Fingerprint-matched IP from the tested HLS/AXI pipeline, followed by board
  synthesis, routing, bitstream/XSA export and SDT generation. Timing passes:
  WNS +2.380 ns, WHS +0.005 ns. Existing methodology/external-I/O qualification
  limitations are not erased by this successful build.
- Linux 6.18.10, built-in UART Lite (`CONFIG_SERIAL_UARTLITE=y`), UART Lite
  console disabled, PS UART console retained. Updated `dmotor_access` compiled
  for ARM against this target kernel, not merely the host headers.
- Current DMOTOR runtime, shared monitor codec/transport and tests staged from
  the development source. Target QEMU semantics, including the monitor codec,
  passed. Package manifest contains dmotor-runtime, dmotor-access, the versioned
  kernel-module-dmotor-access package and dvrmc-firmware.
- Boot firmware: all 2,910 tasks succeeded. Linux image: all 9,178 tasks succeeded.
  The export contains software/hardware provenance extracted from the built
  filesystem. Both fingerprints match the current checkout; all exported
  SHA256SUMS entries were checked successfully.

## Outputs

Paths below are relative to the companion `edf-dev` checkout:

- Installed hardware manifest: `config/hardware/arty-z7-20.json`
- Hardware bundle:
  `artifacts/arty-hardware/20261104T231424Z-88b1f40c/hardware-bundle.tar.gz`
- SD export directory:
  `artifacts/arty-z7-20/20261105T000706Z-ce4bbc9b/`
- Compressed disk image: `sdcard.wic.xz` (about 157 MiB); block map:
  `sdcard.wic.bmap`. The uncompressed disk is 4,337,499,136 bytes.
- The same export includes boot.bin, system.dtb, uImage, rootfs.tar.gz,
  packages.manifest, build-info.json and SHA256SUMS.

Directory timestamps are UTC; this build completed on November 4 in
America/New_York. The build records the uncommitted development tree with source
hashes; no repository commit was created by this workflow.

[Retained build manifest](evidence/current_build_2026_11_04/build-info.json),
[UART kernel settings](evidence/current_build_2026_11_04/uart_kernel_config.txt),
[ARM driver compilation](evidence/current_build_2026_11_04/arm_driver_compile.log),
[ARM semantic tests](evidence/current_build_2026_11_04/arm_semantics.log), and
[timing](evidence/current_build_2026_11_04/timing.rpt) provide the verification record.
