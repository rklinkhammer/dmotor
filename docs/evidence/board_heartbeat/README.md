# Board heartbeat implementation checks — 2026-11-08

- `board_status.txt`: isolated RTL switch/latch/filter/LED regression.
- `controller_axi.txt`: full controller, atomic HLS and actual C++ PS co-simulation;
  adds running-drive maintenance inhibit, held latch, rejected enable, STATUS,
  core-reset preservation and disabled explicit acknowledgement checks.
- `controller_cleanup.txt`: full controller cleanup regression.
- `software-tests-current.txt`: clean native software build, 33/33 passed.
  An initial run against old `build/product/software` binaries failed the updated
  acquisition PTY rejection assertion; a clean build of current sources passed.
- `bench-build.txt`: ESP32 UART/U1 image with indicator support built successfully.
- `esp32-build.txt`: full monitor adapter compile/link harness built successfully.
  This harness is not a deployable motor-test application.
- `ip-export.txt`, `project.txt`, `pins.txt`: Vivado 2026.1 IP integrity, Arty saved
  block-design validation/wrapper generation and pin/interface checks passed.

Build directories: `build/board_heartbeat/{bench,esp32,software,vivado}`.
Co-simulation outputs: `build/product/native/ps_controller_{axi,cleanup}`.
RTL unit test outputs: `build/board_heartbeat`.

No implementation/place-and-route, bitstream, timing closure, physical flashing,
GPIO waveform or LED behavior is claimed. No physical gate is closed by these
checks. See `docs/board_heartbeat.md` for deployment and fixture validation.

## Pin-ownership correction

The first EDF implementation reached routing but bitgen failed NSTD-1/UCIO-1.
Its seven preceding 12-1411 warnings identified the root cause: legacy
`rgb_led_tri_o[5:0]` and `sws_2bits_tri_i[0]` already occupied the new LED/SW0
pins through board-generated constraints. The initial static check and BD
validation did not detect that duplicate ownership. The corrected saved BD
removes both legacy external interfaces and disables GPIO0 board automation
and its switch-input channel. The static checker now rejects their return.
No DRC severity is downgraded. See `pinfix-validation.txt` for follow-up checks.

## Subsequent operator hardware confirmation

[Physical report, 2026-11-08](physical_confirmation_2026_11_08.md) records PASS
visual synthetic-HB/LED subcases after the build-only checks above. Their earlier
“no flashing/physical behavior claimed” statements describe those build checks,
not the current cumulative status. U1 is PARTIAL; G1b/G2 remain OPEN.
