# DMOTOR ESP32-S3 firmware — planned repository location

**2026-11-08:** Standalone INA bench measurements and the revised **16 ms monitoring
cycle with 25% reserved margin** are recorded in [the timing record](../../docs/ina_bench_timing.md).
The acquisition core and timing diagnostic now implement the revised envelope; operational
monitor implementation and assembled-board qualification remain open.

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
UART mode and native-USB GPIO/bounded synthetic-HB modes are implemented.
See the [U1 procedure](../../docs/u1_gpio_bench.md); physical U1 results remain pending.

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

The [G1c design tables](../../docs/g1c_monitor_design.md) now record the candidate
budget, storage design and pinned-IDF audit, with remaining review decisions.
Before operational implementation, accept the absolute acquisition-cycle budget, ISR-safe
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

A first operational acquisition **component candidate** now exists in
`components/acquisition/` (portable clock/bus/deadline core) and
`components/monitor_i2c/` (ESP-IDF adapter). This is not the operational application.
See [deadline contract, tick analysis, build/test and target qualification](../../docs/g1c_i2c_deadline_adapter.md).
`tests/` contains portable native and ARM-QEMU policy tests; `build_check/` is a non-deployable IDF compile/link harness.
The new monitor_safety adapter writes HB and permits LOW only. Its physical-health
and safety-core candidates are described below; full policy/recovery integration
remains mandatory before operational use.

The isolated [INA + battery ADC timing diagnostic](diagnostics/ina_timing/README.md)
now prepares the actual transport/ADC workload: two battery ADC channels followed
by twelve register reads using **one INA226 as four logical slots**. Configurable
ADC sampling and optional four IPROPI channels cover the additional analog load.
It captures per-call/cycle timestamps in RAM, prints CSV afterward, and keeps HB
and all motor permits LOW. Separate 100 Hz raw-timeout and 1 kHz bounded profiles
characterize the timeout hazard without changing the operational adapter. All four ESP32-S3 profiles now pass clean target builds on merged main; see
[build evidence](../../docs/evidence/esp32_main_merge/inputs.json). No flash, hardware
timing measurement or safety acceptance is recorded.

## Implementation follow-up — 2026-11-08

See [receiver rejection reporting, physical sensor health and 16 ms safety candidate](../../docs/monitor_next_steps.md).
Host tests and inhibited firmware builds advance implementation evidence only.
Existing hardware results remain tied to their original diagnostic images;
operational protection, electrical acceptance and live recovery are not claimed.
