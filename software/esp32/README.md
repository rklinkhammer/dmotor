# DMOTOR ESP32-S3 firmware — planned repository location

The ESP32-S3 monitor firmware **will be included in this dmotor repository**.
`software/esp32/` is its implementation root. The operational monitor application is not implemented yet. A standalone
[UART bench diagnostic](diagnostics/bench/README.md) now builds with ESP-IDF.

The firmware owns sensor acquisition, protection policy, physical ARM/STOP,
permit outputs, health-qualified heartbeat and monitor telemetry/recovery.
Zynq Linux owns motion intent; PL owns deterministic motor control. The
[joint recovery contract](../../docs/recovery_transition_contract.md) is the
current proposed cross-device behavior. The earlier monitor/FIRMWARE.md in
motor-interface-integrated is historical design input where it conflicts.

An independent inhibited diagnostic project is implemented at
`diagnostics/bench/` for early UART connectivity and then GPIO/HB observations.
It initializes all permits low and has no ARM path; default UART mode leaves HB
off. See the [bench program plan](../../docs/bench_diagnostic_plan.md). This small
application can be implemented before full acquisition/calibration/protection
policy is finished; it cannot stand in for operational firmware qualification.
UART-only mode is implemented and built; GPIO/HB modes remain planned.

Planned layout:

- `main/`: ESP-IDF startup and board adapter; initialize permits low first.
- `components/safety_core/`: host-compilable C state machine, no IDF dependencies.
- `components/monitor_io/`: GPIO/ISR, ADC, I2C, UART and watchdog adapters.
- `tests/`: native state-machine/protocol tests and scripted peripheral failures.
- Board configuration, pinned IDF version/sdkconfig, partition table and build
  instructions will be tracked here when implemented.

Place the versioned cross-device UART schema and golden wire vectors under
`software/protocol/monitor/` so Linux and ESP32 consume one definition. Existing
`software/protocol.md` describes the FPGA MMIO adapter, a different protocol.
Keep firmware builds separate from the existing native/Linux CMake build;
building Linux must not require ESP-IDF or flash a device.

Before implementation, finish the absolute acquisition-cycle budget, ISR-safe
resource rules, configuration/calibration schema and threshold policy. Fault
logs use bounded RAM first; NVS writes, calibration and firmware updates occur
only inhibited. Validate the population mask, always require both battery taps,
and never silently remove a missing sensor. Missing/corrupt calibration blocks
arming. Do not choose electrical thresholds merely to make tests pass.

Release evidence must identify dmotor commit, ESP-IDF/toolchain configuration,
firmware image, protocol version, board revision and calibration generation
alongside the Zynq software/bitstream identity. Native tests are not evidence of
electrical timing or powered-motor qualification.

PCB, fabrication and assembly sources remain in motor-interface-integrated.
That repository may link here, but must not carry an independent firmware fork.
