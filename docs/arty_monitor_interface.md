# Arty motor/monitor interface — saved BD integration, 2026-11-03

Current board-status addition: [SW0 maintenance inhibit, LD4/LD5 and ESP32 heartbeat LEDs](board_heartbeat.md). Synthetic HB/LED visual checks are operator-confirmed; see [operator-confirmed synthetic heartbeat/LED checks](evidence/board_heartbeat/physical_confirmation_2026_11_08.md). Electrical timing and full interlock qualification remain pending.

The saved block design supports controller revision 10, four motor channels, an
internal drive-enable GPIO readout, and AXI UART Lite for the ESP32 monitor.
The [2026-11-04 full EDF build](current_build_2026_11_04.md) includes these
assignments and the 115200 UART. No board has been programmed or electrically
qualified by that build.

## Motor wiring

Each motor-board J3 is: 1 FPGA_3V3, 2 PWM, 3 DIR, 4 QA_3V3, 5 QB_3V3,
6 GND. These six-pin connectors require mapped harnesses, not straight-through
Pmod cables. Each Pmod carries two motors' signals. Pmod pins 5/11 are ground;
6/12 are 3.3 V. Do not confuse the driver EN input (fed by PWM) with the
controller's separate drive_enable diagnostic outputs.

| Motor | DIR | PWM | QA input | QB input |
|---|---|---|---|---|
| 0 | JA-1 / Y18 | JA-2 / Y19 | JA-3 / Y16 | JA-4 / Y17 |
| 1 | JA-7 / U18 | JA-8 / U19 | JA-9 / W18 | JA-10 / W19 |
| 2 | JB-1 / W14 | JB-2 / Y14 | JB-3 / T11 | JB-4 / T10 |
| 3 | JB-7 / V16 | JB-8 / W16 | JB-9 / V12 | JB-10 / W13 |

## Monitor and diagnostic wiring

| Monitor connector | BD signal | Current pin assignment |
|---|---|---|
| J7-1, J7-6 ground | GND | Board ground |
| J7-2 HB_OUT | mon_hb → controller hb_async | CK_IO2 / U13 |
| J7-3 HALT_IN | controller coast_req → coast_req | CK_IO3 / V13 |
| J7-4 monitor TX | mon_rx → UART Lite rx | CK_IO1 / U12 |
| J7-5 monitor RX | UART Lite tx → mon_tx | CK_IO0 / T14 |

On 2026-11-04 the user confirmed the saved assignments: CK_IO2/U13 for mon_hb
and CK_IO3/V13 for coast_req, superseding CK_IO4/5. CK_IO2/U13 is the
PUDC_B-related pin; this assignment does not establish configuration-time coast
or reset safety. All four
monitor signals use 3.3 V logic; J7 supplies no power connection.

AXI GPIO 3 channel 1 remains the one-bit PS HALT output to halt_async. Channel 2
is a four-bit input connected to drive_enable, providing diagnostic readback
without consuming external pins. Revision 10 registers the source in the 20 MHz
controller domain before GPIO synchronizes it to 100 MHz. The readout trails the
raw drive state by one controller clock plus GPIO synchronization latency;
individual bits are diagnostic, not an atomic snapshot or physical-permit measurement.
The controller retains four system-level `physical_faults[3:0]` inputs, independent
of motor identity; source 0 is assigned to bumper contact and 1–3 are reserved.
See [source assignments and telemetry mapping](controller_axi.md#system-physical-fault-sources).
The saved BD currently drives all
four low with a constant. Future external fault routing requires active-high
sources and defined inactive levels. Motor-board nFAULT goes to the ESP32.

## UART configuration and software boundary

UART Lite is explicitly configured for **115200 baud, 8 data bits, no parity**
(one stop bit). Its AXI clock is FCLK_CLK0 at 100 MHz, not the motor controller's
20 MHz clock. Its register range is 0x42C00000 / 64 KiB; its interrupt connects
to ilconcat_0/In0 and onward to the PS fabric IRQ input. Preserve that mapping
in the exported hardware description; do not guess a Linux tty number.

The saved PS GP0 frequency metadata is corrected from 10 to 100 MHz to match
its actual FCLK_CLK0 connection; this does not change the clock generator.
The bitstream and exported hardware/device-tree bundle must be regenerated
together before deploying the UART. Linux needs UART Lite driver support and
a node from the matched hardware description. It must not be configured as a
console/getty when used for monitor recovery. The PS UART transport is implemented; the full recovery exchange remains pending.
See [current status](system_status.md).

## Checks

Run the source interface check:

```sh
python3 tools/check_arty_interface.py
```

The check verifies motor concatenation/slices, monitor directions/connectivity,
drive-enable GPIO, UART parameters and explicit nonduplicated 3.3 V pins. The
optional --allow-unassigned mode reports pending pins during development; it is
not a bitstream release gate. The strict check passes with all 20 signal pins
assigned, including the user-selected HB/coast pins.

Vivado project recreation and BD validation were run in the separate generated
directory `build/monitor_board_check/vivado`, preserving the user's open product
project. Generated UART parameters confirm 115200/8/no parity and 100 MHz AXI
clock. Revision 10 subsequently completed synthesis and routing with positive
setup/hold margins and no critical CDC findings after registering drive-enable
diagnostics. See [PL progress and retained reports](archive/pre-system-plan-2026-11-04/docs/pl_monitor_progress.md#routed-revision-10-follow-up)
for remaining methodology/I/O timing warnings and release limitations. The XDC
assignment alone does not establish coast during configuration, power loss or
stopped PL clocks.
