# DVRZ platform — MYIR Z-Turn XC7Z020

Imported on 2026-10-19 from `~/dvrz/dvrz.xpr`, preserving its saved BD and XDC.
See [build commands](build.md#additional-platform-dvrz--myir-z-turn).

- Device: `xc7z020clg400-1`; board: `myir.com:mys-7z020:part0:2.1`.
- Project/top: `dvrz` / `dvrz_wrapper`.
- PS clocks: FCLK0 100 MHz; FCLK1 20 MHz for the controller.
- Shared `build/current_ip/controller_axi/ip` is upgraded during recreation.
- Four motor channels are exposed. Four-bit NOT blocks invert physical fault
  inputs and drive enables for the adapter. Host HALT originates at AXI GPIO 2.
- Board PS settings are preserved; no Arty preset is applied.

## Pending integration

User requested that pin assignments remain pending. The imported XDC is retained
verbatim, including obsolete `mc0_qcA`/`mc0_qcB` and `mc1_qcA`/`mc1_qcB` names,
and second bits of direction ports that are now one-bit ports. Several current
PWM, enable, fault and encoder ports have no assignments. Resolve this against the
actual connector wiring; do not reuse Arty package-pin assignments. Review pull
resistors for the inverted fault/enable polarity before wiring a driver.

The saved BD exposes `mc_halt` but has no net connecting it to the controller's
`shared_halt` output. This connection must be completed before shared external
HALT signaling can be relied upon.

Vivado emitted BD 41-2162 while importing the saved design: it could not find
`processing_system7_0/IRQ_F2P` while restoring the interrupt net. Review the
regenerated PS fabric interrupt configuration and connections before Linux use.

After correcting topology and constraints, perform synthesis, implementation,
timing/DRC and physical I/O checks. EDF platform configuration, hardware bundle
and device-tree integration are separate follow-up work. No powered validation
or board programming was performed during this import.

## Validation

Vivado 2026.1 direct invocation of the new `create.tcl` completed with exit code 0:
BD open/upgrade/validation, output-product generation, wrapper creation and IP
status report. Generated project: `build/product/dvrz/vivado/dvrz.xpr`; report:
`build/product/dvrz/vivado/ip_status.txt`. It used the existing packaged controller
IP; this check did not rerun HLS, synthesize or route the board. Initial missing
XCI warnings were followed by regeneration of BD IP output products. The interrupt
warning above remains an integration finding, not a clean-validation claim.

Build-wrapper dry runs verified default Arty and explicit DVRZ selection use the
same IP fingerprint and distinct project destinations. Original BD/XDC import
copies match the source project byte-for-byte.

## GUI editing

Open the generated DVRZ project. The XDC references the tracked file directly.
After BD edits, save and locate it with `get_files */dvrz.bd`; copy the saved BD
back to `platforms/dvrz/vivado/dvrz.bd` and review the diff. Close the project before
recreation, which replaces generated output. Do not edit the original `~/dvrz`
project expecting those edits to be picked up automatically.
