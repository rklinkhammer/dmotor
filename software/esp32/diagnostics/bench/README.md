# ESP32-S3 UART bench diagnostic

This is a standalone ESP-IDF application, not the operational monitor firmware.
GPIO8/9/10/11 permits and GPIO21 HB are driven low before UART startup. There is
no ARM path, sensor acquisition or synthetic heartbeat mode. External pulldowns
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

Firmware answers diagnostic requests only. Each response contains its own
sequence-dependent reverse-direction pattern, not an echo. Runs have random
64-bit IDs chosen by the Linux utility; a new run must start at sequence 1.
Within a run, repeated/skipped sequences are rejected. Counters are cumulative
since ESP32 boot, printed after 1.5 seconds without a valid request. A new run
never clears observed error counters. TX failure stops service until reboot,
with permits/HB still low. Frames and I/O work are bounded; maximum run sequence
is 1000. The endpoint listens for another explicitly started run while idle.

See [bench instructions](../../../../docs/uart_bench.md) from the repository docs.

For an existing build configured for UART console, use `menuconfig` → Component
config → ESP System Settings → Channel for console output → USB Serial/JTAG;
select no secondary console, then rebuild. Fresh builds use sdkconfig.defaults.
Defaults alone do not override an existing sdkconfig.
