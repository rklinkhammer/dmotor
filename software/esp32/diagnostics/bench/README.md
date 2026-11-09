# ESP32-S3 UART / GPIO / HB bench diagnostic

For the narrow-breadboard **UART/U1** heartbeat test, use the separate [GPIO3 pre-PCB profile](../../../../docs/board_heartbeat.md#pre-pcb-breadboard-variant-hb-on-gpio3). PCB/operational HB remains GPIO21; the INA acquisition image remains inhibited.

Current board-status addition: [SW0 maintenance inhibit, LD4/LD5 and ESP32 heartbeat LEDs](../../../../docs/board_heartbeat.md). Synthetic HB/LED visual checks are operator-confirmed; U1 remains partially complete. See the linked procedure for the evidence and remaining electrical/interlock checks.

**Pre-PCB implementation update (2026-11-08):** [Concurrent acquisition/UART diagnostic](../../../../docs/prepcb_acquisition.md)
adds fixed-memory long runs, a Zynq tty receiver, synthetic/error-injection profiles,
and task-level deadline observation. Existing standalone 100/100 physical evidence
remains valid for its original image. The concurrent run now reports 10,000/10,000
acquisitions and 1,000/1,000 Zynq replies, no failures/skips/expirations, max
acquisition 8.215 ms. Delayed injection also passed its diagnostic criteria: 400 expected failures,
400 skipped releases and 400 observer expirations; Zynq rejected the supplied
snapshots. Reboot and two UART-loss cases now show expected receiver timeouts after valid
samples. An explicit restored-link run passed 1,000/1,000 replies with zero errors in
69.999 s. I/O-only injection also passed: 400 expected failures, zero skipped releases
and zero observer expirations, with Zynq rejecting the supplied snapshots. Permits/HB remain inhibited; no operational health or recovery is claimed.

For INA226/battery ADC tests 2 and 3, use the separate
[INA diagnostic](../ina_timing/README.md) and
[measured timing / revised acceptance procedure](../../../../docs/ina_bench_timing.md).
This UART/GPIO program has no I2C acquisition; its synthetic HB timing is unchanged.

This is a standalone ESP-IDF application, not the operational monitor firmware.
GPIO8/9/10/11 permits and GPIO21 HB are driven low before UART startup. There is
no ARM path or sensor acquisition. Explicit native-USB commands now support
raw GPIO reporting and a bounded synthetic heartbeat. External pulldowns
cover the period before app_main runs. Physical startup levels still need measurement.

From the dmotor repository, with esp32-dev beside it:

```sh
../esp32-dev/esp32 --project software/esp32/diagnostics/bench --target esp32s3 build
```

After checking the inhibited bench setup and selecting the actual USB device,
flash explicitly (replace `/dev/ttyACM0` if different):

```sh
../esp32-dev/esp32 --project software/esp32/diagnostics/bench --target esp32s3 --port /dev/ttyACM0 flash monitor
```

Build never flashes automatically. Logging and the IDF console use the ESP32-S3
native USB Serial/JTAG port (normally `/dev/ttyACM0`), with no secondary UART
console. GPIO43/44 are not used for application logging. Test traffic alone uses
UART1 GPIO17 TX / GPIO18 RX, 115200 8N1, to the Zynq through J7.
Use the native USB connector for both programming and `monitor`; exit monitor
with Ctrl+]. If USB re-enumerates after reset, select its new device and restart
monitor. Do not parallel USB and external
supplies without checking the actual board power arrangement.

UART1 answers diagnostic requests only. Each response contains its own
sequence-dependent reverse-direction pattern, not an echo. Runs have random
64-bit IDs chosen by the Linux utility; a new run must start at sequence 1.
Within a run, repeated/skipped sequences are rejected. Counters are cumulative
since ESP32 boot, printed after 1.5 seconds without a valid request. A new run
never clears observed error counters. TX failure stops service until reboot,
with permits low and synthetic HB revoked. Frames and I/O work are bounded; maximum run sequence
is 1000. The endpoint listens for another explicitly started run while idle.

See [bench instructions](../../../../docs/uart_bench.md) from the repository docs.

For an existing build configured for UART console, use `menuconfig` → Component
config → ESP System Settings → Channel for console output → USB Serial/JTAG;
select no secondary console, then rebuild. Fresh builds use sdkconfig.defaults.
Defaults alone do not override an existing sdkconfig.

## U1 commands on native USB

Type `help`, `gpio`, `hb10`, `stop` or `quiet`, followed by Enter. No input echo.
`hb10` explicitly starts a nominal 50 Hz synthetic GPIO21 heartbeat for 10 seconds;
repeated starts cannot extend it. All permits remain LOW. `stop` cancels it;
`quiet` only hides reports. USB disconnect, missed edge deadline, expiry or terminal
UART TX failure stops HB. Raw STOP/HALT inputs do not gate this diagnostic HB.

U1 requires `CONFIG_FREERTOS_HZ=1000`. Fresh defaults select it; an existing
sdkconfig must be updated using menuconfig if the compile-time check fails.
See the [U1 procedure](../../../../docs/u1_gpio_bench.md) for wiring, expected
reports and the distinction between bare-node tests and assembled-board tests.

Physical U1 testing is paused pending assembled monitor/motor boards at Robert's
request (2026-11-06). Build evidence is recorded; no U1 flash/measurement result
is recorded. Retain these commands for resumption; U0 results remain valid.
