# DMOTOR

Shared four-motor FPGA velocity controller for Zynq-7000, with processor-owned
motion planning. The current product profile is 20 MHz, up to 400 RPM per the
configured motor limits, using the exact one-phase shared arithmetic implementation.

- [System execution status](docs/system_status.md), [decisions](docs/decisions.md), and [open issues](docs/open_issues.md).
- **[Build guide](docs/build.md)** — current component, board, GUI and Linux workflow.
- [Documentation index](docs/index.md) — architecture, implementation and evidence.
- [Architecture](DMOTOR_ARCH.md), [integrated execution plan](docs/system_bringup_plan.md) and [external monitoring circuitry](DMOTOR_CONTROL.md).
- [Maintenance/commissioning](software/linux/production.md).
- [ESP32-S3 firmware plan](software/esp32/README.md) — firmware and tests will
  live in this repository under `software/esp32/`; implementation is pending.

Board integration formerly maintained as DVRMC now lives in
`platforms/arty-z7-20`. Generated IP/projects live under `build/`; one tracked XDC
is authoritative. EDF consumes this checkout for both hardware and host software.
Physical motor qualification remains distinct from build and simulation success.

Additional board: [DVRZ / MYIR Z-Turn](docs/dvrz.md), selected with `tools/build.py project --platform dvrz` (pin assignments pending).
