# Board heartbeat and maintenance inhibit

**Current fixture (2026-11-10):** The operator now has built motor and monitor
boards. Return HB to **GPIO21** using the normal UART/U1 PCB diagnostic image
(`build/board_heartbeat/bench`), with the pre-PCB GPIO3 option disabled. The
operational monitor already uses GPIO21. The RGB LED remains GPIO48. GPIO3
firmware is retained only for the earlier breadboard fixture; do not flash that
variant for the PCB HB connection. Board availability is not gate acceptance.

Implemented in source for Arty Z7-20. Controller IP packaging and saved block-design
validation are checked. The pin-ownership correction also passed complete
synthesis and all seven synthesized pin/IOSTANDARD assertions. The operator has
now confirmed the synthetic HB/LED visual checks on hardware; see the
[physical record](evidence/board_heartbeat/physical_confirmation_2026_11_08.md).
Loaded image identity and measured electrical timing were not supplied. Full
interlock and matched-release qualification remain open.

## Signals and indicators

| Controller port | Arty resource | Meaning |
| --- | --- | --- |
| `maintenance_async` | SW0, M20 | HIGH requests maintenance inhibit |
| `led4_r/g/b` | LD4, N15/G17/L15 | Red: inhibited or no active drive authorization; blue: drive authorized |
| `led5_r/g/b` | LD5, M15/L14/G14 | Red: received HB not healthy; blue: received HB healthy |
| Existing `hb_async` | CK_IO2, U13 | ESP32 GPIO21 heartbeat input |
| Existing `coast_req` | CK_IO3, V13 | Includes maintenance inhibit as well as existing coast causes |

The legacy `rgb_led` and `sws_2bits` external AXI GPIO interfaces are removed.
SW0 feeds only the controller interlock; switch readback is removed and SW1 is
unassigned. AXI GPIO0 remains at its existing bus address with its unused output
channel, but no longer controls physical LEDs or exposes a switch-input channel.
This prevents board automation from assigning the same pins twice.

Both RGB indicators flash at approximately 2 Hz using the independent 20 MHz PL
clock. Green is unused. LD4 blue means the controller is authorizing at least one
drive channel; it is not proof that a motor is moving. Disabled, zero-duty and
faulted states can remain red with SW0 off. LD5 uses the heartbeat supervisor's
`healthy` observation, not its fault latch or motor authorization: blue can coexist
with LD4 red. The existing 50 ms HB edge timeout, minimum edge spacing and 500 ms
qualification window are unchanged. During reset the blink phase is dark.

SW0 assertion passes through a two-flop synchronizer without assertion debounce,
then inhibits all motor outputs and asserts coast. Any sampled assertion latches
inhibit. Release must remain stable for 20 ms, then requires an explicit
`ACKNOWLEDGE` bit 1 (write value 2 at offset `0x1024`) with a nonzero session,
`CORE_RESET=1`, and all motors disabled. Assertion wins over acknowledgement.
The latch starts inhibited at FPGA reset. Core reset alone does not clear it.
Existing daemon prepare already issues this acknowledgement in its disabled/reset
sequence. No new register offsets or UART frames are introduced; the existing
aggregate STATUS fault bit includes maintenance inhibit, and ENABLE rejects while
maintenance inhibit is active. Normal reconfiguration/prepare and authorization
are still needed afterward; releasing SW0 never restarts motors.

This interlock depends on a configured and clocked PL. It does not provide
configuration-time isolation during bitstream loading. Initial deployment needs a
new matched bitstream/overlay; subsequent software maintenance can use SW0 without
changing the FPGA image. Determine the slider position producing HIGH on the board.

## ESP32 heartbeat and LED

`MonitorSafety` now writes its qualified heartbeat state to GPIO21. One edge is
produced per valid completed 16 ms monitoring cycle (nominal full waveform 31.25 Hz),
independently of ARM and motor permission. Missing/invalid/late monitoring drives
HB LOW. Motor permits GPIO8–11 remain hard-inhibited in this integration candidate.
The standalone one-INA health diagnostic lacks battery/calibration/full-policy
proof and therefore still keeps HB LOW. It is not an operational heartbeat source.

The addressable onboard RGB LED now defaults to **GPIO48**, selected for the
current bench setup. Set `DMOTOR_HB_LED_GPIO=38` for boards with the LED on GPIO38
(DevKitC-1 v1.1). Existing saved sdkconfigs retain their selection; the UART/U1
config at `build/board_heartbeat/bench/sdkconfig` has been changed to 48.
Only 38 and 48 are accepted; other
values disable the indicator. A low-priority RMT task displays flashing blue while
locally generated HB edges continue and flashing red after 50 ms without an edge.
It observes output commands, not a physical GPIO readback. Indicator failures do
not change heartbeat or permits. The PL indicator independently observes the wire.

## First hardware validation: existing U1 diagnostic

Use the UART/U1 bench application, **not** the INA timing or one-INA health images.
Its `hb10` command remains an explicitly synthetic 50 Hz signal for at most ten
seconds; it now also drives the local RGB indication. It does not demonstrate
sensor health or authorize motion. Keep motor drive power disconnected and SW0
asserted throughout this wiring test.

From the dmotor repository on the build host:

```sh
../esp32-dev/esp32 --project software/esp32/diagnostics/bench --target esp32s3 idf \
  -B "$PWD/build/board_heartbeat/bench/build" \
  -D SDKCONFIG="$PWD/build/board_heartbeat/bench/sdkconfig" \
  -D SDKCONFIG_DEFAULTS=sdkconfig.defaults build
# Use the same build/config arguments with menuconfig to select the LED GPIO.
../esp32-dev/esp32 --project software/esp32/diagnostics/bench --target esp32s3 \
  --port /dev/ttyACM0 idf \
  -B "$PWD/build/board_heartbeat/bench/build" \
  -D SDKCONFIG="$PWD/build/board_heartbeat/bench/sdkconfig" flash monitor
```

Build the matching PL through `python3 tools/build.py project`, then the existing
[EDF bitstream and deployment workflow](build.md). The project command alone does
not produce a deployable bitstream. The compile-check ESP32 app is not a test image.

1. With SW0 asserted, LD4 must flash red. Before HB activity LD5 and the ESP32 RGB
   indicator should flash red.
2. Send `hb10` on the native USB console. ESP32 RGB should flash blue. LD5 should
   turn flashing blue after its qualification interval. LD4 must stay red.
3. Send `stop`, or let ten seconds elapse. Both HB indicators return to flashing
   red after their observation timeouts. Static HIGH and LOW must not qualify.
4. Disconnect/reconnect the HB wire and repeat; record GPIO21 and CK_IO2 waveforms,
   qualification/loss timing and LED behavior. A local blue LED with PL red points
   to a receive-path/qualification issue, not necessarily bad sensor health.
5. With motors unpowered, verify SW0 assertion inhibits a commissioned test channel;
   release alone leaves it inhibited. Only disabled/reset explicit prepare may
   clear the latch. Reassert SW0 during acknowledgement to verify assertion wins.

The operator-confirmed `hb10` blue indication, stop/loss red indication and
SW0-asserted LD4 red indication are PASS visual subcases. The checklist above
retains additional unverified measurements; confirmation does not close every step.
Record firmware and bitstream hashes when available. Measured waveform/latency,
permit and SW0 output/rearm checks, four-sensor monitoring, calibration, ARM and
recovery gates remain open.

## Pre-PCB breadboard variant: HB on GPIO3

For the narrow-breadboard fixture only, the UART/U1 diagnostic has a separate
`CONFIG_DMOTOR_PREPCB_HB_GPIO3=y` profile. Connect **ESP32 GPIO3 → Arty CK_IO2/U13**
with common ground. The Zynq pin and bitstream do not change. GPIO21 stays LOW in
this variant; motor permits stay LOW. The RGB LED remains on GPIO48.

This option exists only in the UART/U1 bench application. The default PCB bench
profile and operational monitor keep HB on GPIO21. The INA acquisition diagnostic
still does not produce healthy HB; this option does not add that behavior.

Build and flash the separate pre-PCB image from `dmotor`:

```sh
../esp32-dev/esp32 --project software/esp32/diagnostics/bench --target esp32s3 idf \
  -B "$PWD/build/prepcb_hb3/esp32/build" \
  -D SDKCONFIG="$PWD/build/prepcb_hb3/esp32/sdkconfig" \
  -D SDKCONFIG_DEFAULTS=sdkconfig.prepcb.defaults build
../esp32-dev/esp32 --project software/esp32/diagnostics/bench --target esp32s3 \
  --port /dev/ttyACM0 idf \
  -B "$PWD/build/prepcb_hb3/esp32/build" \
  -D SDKCONFIG="$PWD/build/prepcb_hb3/esp32/sdkconfig" flash monitor
```

The boot banner and `help` identify the actual HB GPIO. Check that they say
GPIO3 before using `hb10`. With SW0 asserted and motor power disconnected, verify
blue HB indication, then red on `stop`/loss, with LD4 staying red. The earlier
GPIO21 visual PASS does not qualify this new GPIO3 wiring; its physical check is
pending. Return to the normal `build/board_heartbeat/bench` image for PCB wiring,
or disable the pre-PCB option in that image's menuconfig and rebuild.

## Software validation

The regression suite covers startup inhibit, switch assertion, release filtering,
held-switch acknowledgement, no automatic restart, LED states and full-controller
maintenance interruption/recovery. Evidence is under `docs/evidence/board_heartbeat/`.

## Rebuilding after the pin-ownership correction

The failed EDF run's project contains the old exported RGB/switch interfaces.
Do not resume implementation in that retained project. From `edf-dev`, recreate
the hardware project through the normal workflow:

```sh
EDF_DMOTOR_SOURCE=../dmotor ./edf --target arty-z7-20 hardware
```

This regenerates the board project from the corrected saved BD. No DRC severity
override is needed. The current source keeps SW0/LD4/LD5 exclusively under the
controller and removes the old PS switch readback.
