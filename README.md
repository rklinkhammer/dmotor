# DMOTOR

Shared four-motor FPGA velocity controller for Zynq-7000, with processor-owned
motion planning. The current product profile is 20 MHz, up to 400 RPM per the
configured motor limits, using the exact one-phase shared arithmetic implementation.

- **[Build guide](docs/build.md)** — current component, board, GUI and Linux workflow.
- [Documentation index](docs/index.md) — architecture, implementation and evidence.
- [Architecture](DMOTOR_ARCH.md) and [software plan](DMOTOR_SW.md).
- [Maintenance/commissioning](software/linux/production.md).

Board integration formerly maintained as DVRMC now lives in
`platforms/arty-z7-20`. Generated IP/projects live under `build/`; one tracked XDC
is authoritative. EDF consumes this checkout for both hardware and host software.
Physical motor qualification remains distinct from build and simulation success.
