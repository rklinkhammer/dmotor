# Zybo Z7-20 — editable controller integration

Imported from `/home/rklinkhammer/workspace/zybo-7z-20` on 2026-11-08.
The original release is untouched. `import.json` records source hashes and DDR
changes. The user has now wired and validated the saved design. It supports both a GUI
editing project and an EDF build project. Routed hardware and WIC outputs now exist; target boot/DDR acceptance and matched
installed-image identity remain pending. Zybo is the default board (2026-11-12);
[status and current HB detection evidence](../../docs/system_status.md). See [EDF Zybo workflow](../../../edf-dev/docs/zybo.md).

The supplied project targeted Z7-10 / 512 MiB. At the user's direction, this
platform uses `xc7z020clg400-1`, board `digilentinc.com:zybo-z7-20:part0:1.2`, and
the installed Digilent Z7-20 preset's DDR parameters (1 GiB, high address
`0x3FFFFFFF`). The licensed preset XML is retained beside the BD for provenance.
The initial import replaced DDR-related PS parameters. The 2026-11-10 correction
also sets the PS reference oscillator to **33.333333 MHz**, confirmed by the
Digilent downloaded Pcam project and board preset; the imported 50 MHz value was
incorrect. MIO/peripheral choices and desired 100 MHz FCLK0 / 20 MHz FCLK1 are
retained. DDR is not board-tested with the corrected image.

Preserved MIO assignments: UART0 MIO14–15, UART1 MIO48–49, I2C0 MIO10–11 and
I2C1 MIO12–13. The separate `pl_uart0` UART Lite and its existing address remain
in the imported design; decide whether it is needed during integration. Linux
console/device mapping and EDF boot/device-tree support are separate follow-up.

## Open and edit

The initial editable project has been generated here:

```sh
/opt/vitis/2026.1/Vivado/bin/vivado -mode gui \
  build/product/zybo-z7-20/vivado/zybo-z7-20.xpr
```

Open `design_1`. `controller_axi_0` is the current PSF1 controller with its
maintenance and LED ports. The initial import left it unconnected; the saved source now contains the user
connections. Edit mode preserves GUI editing. Build mode validates those inputs,
generates output products and creates the wrapper for the EDF hardware flow.
Your readable names are retained, with these import corrections:

- `mc0..3_dir` and `mc0..3_pwm` are outputs; encoder QA/QB remain inputs.
- `maintenance_async` was added to match the existing SW0/G15 constraint.
- Duplicate Arty CK_IO2/U13 and CK_IO3/V13 constraints were removed. The supplied
  Zybo JD assignments remain: `mon_hb=T15`, `coast_req=T14`.
- `bumper_fault` is assigned to header JD, package pin P14, LVCMOS33.
- Auxiliary `pl_uart0` is retained: TX = JD7 / U14, RX = JD8 / U15, both LVCMOS33.
  These interface ports are included in the strict pin preflight.

## Controller wiring guide

| Existing/readable board signals | Controller port / connection to make |
| --- | --- |
| `mc0..3_qa`, `mc0..3_qb` | Concatenate into `qa[3:0]`, `qb[3:0]`, motor 0 at bit 0 |
| `mc0..3_pwm`, `mc0..3_dir` | Slice `pwm[3:0]`, `direction[3:0]` to individual outputs |
| `mon_hb` | `hb_async` |
| `coast_req` | `coast_req` |
| `maintenance_async` | `maintenance_async` |
| `maintenance_led_r/g/b` | `led4_r/g/b` |
| `heartbeat_led_r/g/b` | `led5_r/g/b` |
| `bumper_fault` | `physical_faults[0]`, with polarity checked; drive unused bits explicitly |
| `mon_hb_led` | Additional single-color indicator; behavior not assigned yet |
| PS control path | Connect AXI to controller, retain existing controller register contracts/address expectations |
| Clock/reset | Controller `clk` at 20 MHz, active-high `reset` from that domain's reset block |
| PS HALT / diagnostics | Provide `halt_async`; route `drive_enable` readback if retained; decide `shared_halt` use |

The imported SmartConnect runs at 100 MHz. Configure the AXI clock-domain crossing
for the 20 MHz controller; do not simply connect its bus while ignoring clock
association. Use the existing Arty integration as a reference for address mapping,
HALT and diagnostic GPIO. Review interrupt routing, board-generated constraints,
remaining unconnected inputs and reset polarity before Validate Design.
MIO UART removes the need for a PL UART for that connection; it does not remove
the dedicated HB/coast protection signals or imply live recovery is implemented.

## Save your edits back to the platform

Vivado edits a **copy** of the BD and XDC inside the generated project. After
saving in Vivado, copy those two files back before regenerating:

```sh
cp build/product/zybo-z7-20/vivado/zybo-z7-20.srcs/sources_1/bd/vivado/design_1.bd \
  platforms/zybo-z7-20/vivado/design_1.bd
cp build/product/zybo-z7-20/vivado/zybo-z7-20.srcs/constrs_1/imports/constraints/zybo-z7-20.xdc \
  platforms/zybo-z7-20/constraints/zybo-z7-20.xdc
```

Review the diff and any added dependencies. Once wiring is complete, validate the
BD, generate the HDL wrapper and integrate synthesis/implementation checks into
this platform workflow. The generator defaults to editing; its optional `build` mode creates the validated
wrapper and output products. EDF invokes build mode automatically.

## Recreate an editing project

With an existing packaged controller IP, generate a *new* output directory:

```sh
/opt/vitis/2026.1/Vivado/bin/vivado -mode batch \
  -source platforms/zybo-z7-20/vivado/create.tcl \
  -tclargs build/zybo-edit-next
```

For the full shared controller IP/software prerequisite pipeline:

```sh
python3 tools/build.py project --platform zybo-z7-20
```

The standard command recreates `build/product/zybo-z7-20/release-vivado`, leaving
the `vivado` editing directory untouched. `--force` also reruns shared IP checks.
The low-level creator refuses an existing project; use a fresh directory.
Arty remains the default platform and its flow is unchanged.

Validation completed: static import checks, negative duplicate-pin regression,
Vivado project creation, saved Z7-20/1 GiB configuration and controller IP catalog
status. The subsequent user-wired source passes build-mode BD validation and wrapper
generation. Routed timing now passes in the latest audited bundle. Physical pin review, DDR
boot and remaining Zybo electrical acceptance remain pending; synthetic HB detection
is operator-confirmed. Existing Arty and
pre-PCB diagnostic results are not transferred to Zybo gates.

## EDF hardware and WIC workflow

From `edf-dev`, with your GUI BD/XDC edits copied back to this platform:

```sh
EDF_DMOTOR_SOURCE=../dmotor ./edf --target zybo-z7-20 build
```

This is the Arty-style end-to-end workflow: IP/project, routed bitstream/XSA,
SDT and matched hardware bundle, boot/Linux image and SD export (`sdcard.wic.xz`
and `.wic.bmap`). `hardware` alone stops after the bundle. See
[EDF Zybo instructions](../../../edf-dev/docs/zybo.md) for stage commands and
UART mapping. No complete Zybo image or board boot is claimed by project validation.

Build mode requires explicit scalar and auxiliary-interface pin assignments before
synthesis. All current external scalar and auxiliary UART ports now have assignments.
The canonical XDC assigns bumper_fault to P14 and PL UART TX/RX to U14/U15;
carry these assignments into any older GUI constraint copy before saving it back.
UART directions are relative to the Zybo: connect JD7 TX to the peer RX, and JD8
RX to the peer TX, with common ground. The PS MIO monitor UART is unchanged.

## Garbled USB serial correction — 2026-11-10

The previous build used a 50 MHz PS reference-clock setting on a board whose
Digilent preset/demo specifies 33.333333 MHz. This misprograms the PLL divisors
and affects CPU, DDR, peripheral and fabric rates; changing terminal baud alone
is not the fix. The source is corrected and the static checker rejects 50 MHz.
The EDF importer also corrects FSBL/boot-domain `stdout-path` and serial aliases
to USB UART1 at 115200, matching Linux.

[Checks and evidence](../../docs/evidence/zybo_clock_console/validation.txt).
Full hardware/boot/image regeneration is required; `image` alone would reuse the
old PS initialization. Keep the terminal at 115200 8N1, no flow control:

```sh
# From edf-dev:
EDF_DMOTOR_SOURCE=../dmotor ./edf --target zybo-z7-20 build
```

Use the newly exported SD image, not the earlier artifact. The current GUI editing
copy may still contain 50 MHz: set the PS reference to 33.333333 MHz before copying
any later GUI edits back to the platform. No camera/demo IP was imported for this fix.
