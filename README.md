# DMOTOR

Four-motor velocity controller for Zynq-7000. The default **1 ms PS executor**
performs fixed-point motor calculations; the 20 MHz PL retains encoder capture,
coherent PWM scheduling and independent protection. Motor limits remain up to
400 RPM. See [the architecture](DMOTOR_ARCH.md) and [PSF1 details](docs/ps_executor.md).

- [System execution status](docs/system_status.md), [decisions](docs/decisions.md), and [open issues](docs/open_issues.md).
- **[Build guide](docs/build.md)** — current component, board, GUI and Linux workflow.
- [Documentation index](docs/index.md) — architecture, implementation and evidence.
- [Architecture](DMOTOR_ARCH.md), [integrated execution plan](docs/system_bringup_plan.md) and [external monitoring circuitry](DMOTOR_CONTROL.md).
- [Maintenance/commissioning](software/linux/production.md).
- [ESP32-S3 firmware plan](software/esp32/README.md) — UART/U1 diagnostics live in this repository; operational monitor firmware remains pending.

Board integration formerly maintained as DVRMC now lives in
`platforms/arty-z7-20`. Generated IP/projects live under `build/`; one tracked XDC
is authoritative. EDF consumes this checkout for both hardware and host software.
Physical motor qualification remains distinct from build and simulation success.

Additional board: [DVRZ / MYIR Z-Turn](docs/dvrz.md), selected with `tools/build.py project --platform dvrz` (pin assignments pending).
