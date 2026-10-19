# Build guide — DMOTOR components and Arty board

This is the authoritative component-build guide. The companion EDF
`docs/build.md` owns Docker, complete system/image export, hardware-bundle import,
SDK and BitBake commands. The previous README and IP-export instructions are in
[history](history/development-log.md); experimental measurement commands elsewhere
reproduce those experiments and are not the current product build.

## Sources and outputs

| Input | Owner / generated output |
| --- | --- |
| HLS/RTL and ABI schemas | DMOTOR `src`, `include`, `rtl`, `abi` |
| Four-motor 400 RPM profile | `tools/prepare_shared_mailbox.py` and its tested transformations |
| Board topology | `platforms/arty-z7-20/vivado/design_1.bd` |
| Board pin/clock constraints | `platforms/arty-z7-20/constraints/arty-z7-20.xdc` |
| Vivado project | Generated `build/product/vivado/dvrmc.xpr` |
| Packaged IP | Generated `build/current_ip/` |
| Software/kernel driver | `software/` |
| Linux/root filesystem | EDF recipes, consuming this same checkout |

The board BD/XDC were migrated from the latest saved root `dvrmc/dvrmc.xpr`
project, as selected by the user on 2026-10-17. It exposes two fault inputs and
two shared-HALT outputs; the current revision-6 controller IP is regenerated automatically. The old checkout is retained as reference;
it is no longer the default build input. No .xpr, .xci or run outputs need to be
copied into the new source directory. Vivado generates them from the tracked BD.

## Current hardware build

On the Linux host with AMD 2026.1 tools in `/opt/vitis/2026.1`:

```sh
python3 tools/build.py project --dry-run
python3 tools/build.py project
# IP only, with all prerequisite profile/HLS/AXI tests:
python3 tools/build.py ip
# Explicitly invalidate cached IP:
python3 tools/build.py project --force
```

Override `--amd-root /path/to/2026.1` if needed. Install Digilent Arty Z7-20
board files version 1.1; `DMOTOR_BOARD_REPO` can point to a board repository if
it is not already in Vivado or its local XHub board store. The pipeline runs profile native
regression tests; atomic-mailbox and shared-engine HLS C simulation, synthesis and
co-simulation; IP packaging; complete AXI simulation; and board project recreation.
The prepared engine is always `build/product/engine`; no implicit selection of an
old `shared_mailbox_copy` or `rpm400_*` directory occurs. Historical transformation
patches remain inputs for now, included in the fingerprint; they are not disposable
build output. Promoting that generated profile to direct sources is separate work.

A successful IP build records its source fingerprint. HLS/RTL/build-test source changes invalidate
reuse; board-only BD/XDC changes do not repeat HLS. Packaged IP hashes are checked; `--force` handles changed external tool installations or suspect outputs.
Board projects are recreated each time. Standalone builds serialize through
`build/.product.lock`; do not run low-level export scripts concurrently.

For complete bitstream/SDT/Linux/SD builds, use the EDF build guide. It calls this
pipeline automatically, runs board implementation and timing/DRC gates, then
creates a portable matched hardware bundle. Ordinary software-only EDF builds do
not invoke HLS/Vivado. A Mac consumes the bundle with Docker/OrbStack.

## Additional platform: DVRZ / MYIR Z-Turn

`--platform` selects the board project; Arty remains the default. Both platforms
use the same tested XC7Z020 controller IP. Board sources are excluded from the
shared IP fingerprint, while the selected project is recreated on every run.

```sh
cd ~/workspace/dmotor
python3 tools/build.py project --platform dvrz --dry-run
python3 tools/build.py project --platform dvrz
/opt/vitis/2026.1/Vivado/bin/vivado -mode gui build/product/dvrz/vivado/dvrz.xpr
```

For direct recreation with already packaged IP:

```sh
/opt/vitis/2026.1/Vivado/bin/vivado -mode batch \
  -source platforms/dvrz/vivado/create.tcl \
  -tclargs build/product/dvrz/vivado
```

DVRZ requires board definition `myir.com:mys-7z020:part0:2.1`; use
`DMOTOR_BOARD_REPO` if it is not installed in the local board store. Tracked sources
are `platforms/dvrz/vivado/dvrz.bd` and `platforms/dvrz/constraints/dvrz.xdc`.
The source project `~/dvrz` is retained untouched and is no longer read by these
commands. DVRZ output does not replace `build/product/vivado` for Arty.

**DVRZ currently supports project recreation, not a qualified bitstream.** Pin
assignments remain pending by user decision; see [DVRZ integration status](dvrz.md).
This addition does not add an EDF target, firmware bundle, or Linux device tree.
The EDF Arty workflow remains unchanged.

## GUI work

### Recreate and open the current project

From the DMOTOR repository, the recommended command prepares/verifies the IP and
invokes `platforms/arty-z7-20/vivado/create.tcl` automatically:

```sh
cd ~/workspace/dmotor
python3 tools/build.py project
/opt/vitis/2026.1/Vivado/bin/vivado -mode gui build/product/vivado/dvrmc.xpr
```

To invoke `create.tcl` directly after preparing the IP:

```sh
cd ~/workspace/dmotor
python3 tools/build.py ip
/opt/vitis/2026.1/Vivado/bin/vivado -mode batch \
  -source platforms/arty-z7-20/vivado/create.tcl \
  -tclargs build/product/vivado
/opt/vitis/2026.1/Vivado/bin/vivado -mode gui build/product/vivado/dvrmc.xpr
```

The script requires exactly one argument: the output directory (relative to the
invocation's working directory, or absolute). It creates project `dvrmc` for
`xc7z020clg400-1`, imports the tracked BD, references the tracked XDC, upgrades the
controller instance from `build/current_ip/controller_axi/ip`, validates the BD,
generates targets/wrapper, writes `ip_status.txt`, and closes the project. It does
not synthesize, implement, or generate a bitstream.

It uses `create_project -force`: save any generated-project BD edits back to the
tracked source first, close its GUI, and do not run this concurrently with a build.
Direct invocation bypasses the build wrapper's lock and IP fingerprint validation.
For an independent GUI project, pass another generated directory, for example
`build/gui/dvrmc`, and open `build/gui/dvrmc/dvrmc.xpr` instead.

If the Arty board definition is missing, set `DMOTOR_BOARD_REPO` to your installed
Digilent board repository before invoking Vivado. The required board identifier is
`digilentinc.com:arty-z7-20:part0:1.1`. The script discovers source/IP paths relative
to its own location; it does not use the legacy `~/workspace/dvrmc` checkout.

### Preserve GUI edits

Open `build/product/vivado/dvrmc.xpr` in Vivado. Edit the source XDC directly;
it is referenced rather than imported. To retain block-design changes, save the
BD, locate its path using the Vivado Tcl command `get_files */design_1.bd`,
then copy that saved file back to
`platforms/arty-z7-20/vivado/design_1.bd` and review the diff. The next build replaces
the generated project. Do not use a GUI project concurrently with its rebuild.
HLS components are `build/vitis_atomic` and
`build/product/engine/build/vitis_dispatcher`; their configs are respectively
`platforms/vitis/atomic.cfg` and the prepared engine's `platforms/vitis/dispatcher.cfg`.

## Software iteration and ABI checks

```sh
cmake -S software -B build/software -DDMOTOR_SW_SANITIZE=ON
cmake --build build/software -j4
ctest --test-dir build/software --output-on-failure
python3 tools/generate_controller_abi.py --check
python3 tools/generate_mailbox_abi.py --check
python3 software/linux/driver/generate_abi.py --check
```

EDF compiles the ARM software and kernel module and runs user-mode QEMU semantic
tests. See its build guide for package/image commands. Building packages does not
update a previously exported image. Runtime operation, access-mode selection and
motor configuration belong in [commissioning](../software/linux/production.md),
not this build guide.

## Cleanup

`build/` contains disposable generated output, including the recreated GUI project.
Save reviewed BD source changes first. Deleting `build/product`, `build/current_ip`
and `build/vitis_atomic` forces regeneration. Preserve `docs/evidence`, input
patches, `platforms/arty-z7-20`, ABI schemas and source files. Never delete build
outputs while a tool is using them. Old experiment directories are not selected
by the product workflow and may be removed when their evidence is archived.

## Migration validation status

The user selected the latest saved DVRMC root interface: two fault inputs and
shared-HALT outputs. The new IP pipeline, cache reuse and bundle tooling passed
validation; see [evidence](evidence/build_workflow/validation.txt). The selected
board passed routing (+2.552 ns setup, +0.011 ns hold), zero DRC errors, and
matched EDF hardware import. Existing DSP pipeline and PS DDR skew warnings remain.
The production Linux image and SD export also passed. The export is recorded in
the evidence above; no physical board was programmed or boot-tested.
