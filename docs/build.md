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
| Four-motor 400 RPM profile | Direct `src/` and `include/dmotor/` sources, executed on PS |
| Board topology | `platforms/arty-z7-20/vivado/design_1.bd` |
| Board pin/clock constraints | `platforms/arty-z7-20/constraints/arty-z7-20.xdc` |
| Vivado project | Generated `build/product/vivado/dvrmc.xpr` |
| Packaged IP | Generated `build/current_ip/` |
| Software/kernel driver | `software/` |
| Linux/root filesystem | EDF recipes, consuming this same checkout |

The board BD/XDC were migrated from the latest saved root `dvrmc/dvrmc.xpr`
project, as selected by the user on 2026-10-17, and subsequently updated for
four motor channels and the ESP32 monitor. The current development controller
IP source is revision 13: PSF1 1 ms execution, retaining CLN1 disabled cleanup
and the heartbeat/global coast interface. See the current evidence in [system status](system_status.md);
source revision alone does not establish a rebuilt or qualified image. Physical-fault
inputs remain on the controller, currently tied inactive in the saved BD;
drive-enable diagnostics connect to internal GPIO. The old checkout is retained as reference;
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
it is not already in Vivado or its local XHub board store. Install Icarus Verilog
with VPI development headers and a C++14 compiler. The pipeline builds atomic
mailbox HLS (C simulation/synthesis/co-simulation), direct product C++ sources,
all current native/RTL tests, actual PS-C++/full-controller AXI co-simulation,
PS software/client/U1 tests, controller IP and the original board project.

The control period is fixed at 1 ms. Native artifacts are in `build/product/native`,
PS binaries in `build/product/software`, and packaged controller IP in
`build/current_ip/controller_axi`. No generated motor-engine profile or second
motor HLS kernel is used. Historical preparation scripts/patches are reference
experiments, not default build inputs. `python3 tools/build.py test` runs the
same required component checks and IP generation without board-project recreation.

A successful IP build records its source fingerprint. HLS/RTL/build-test source changes invalidate
reuse; board-only BD/XDC changes do not repeat HLS. Packaged IP hashes are checked; `--force` handles changed external tool installations or suspect outputs.
Board projects are recreated each time. Standalone builds serialize through
`build/.product.lock`; do not run low-level export scripts concurrently.

For complete bitstream/SDT/Linux/SD builds, use the EDF build guide. It calls this
pipeline automatically, runs board implementation and timing/DRC gates, then
creates a portable matched hardware bundle. Ordinary software-only EDF builds do
not invoke HLS/Vivado. A Mac consumes the bundle with Docker/OrbStack.

## Load the installed FPGA image on Zynq

New EDF images package `dmotor-load-fpga` with `dvrmc-firmware`. On the Zynq,
after boot with motor drive inhibited:

```sh
sudo dmotor-load-fpga --dry-run
sudo dmotor-load-fpga
```

This explicitly programs the installed matching bitstream/overlay with `fpgautil`
and verifies FPGA Manager state. It does not enable motors, autostart at boot, or
support live reload. For installing the helper via SCP on an existing image, see
[EDF board operation](../../edf-dev/docs/arty.md#load-and-inspect).

## Network deployment for future Zynq applications

Per [D09](decisions.md#d09--network-deployment-with-retained-sd-image-workflow-2026-11-08),
future application builds should provide Zynq ARM executables/packages, source/build
identity and checksums, runtime dependencies, and explicit `scp`/SSH deployment
instructions. Host-native executables from `build/product/software` are not Zynq
binaries. Application-only updates should be deployable over the network without
rebuilding an entire SD image when compatible with the installed system.

The current deployment will still use the full image procedure below. Maintain
both paths: network application deployment supplements, and does not retire, the
hardware/EDF/WIC build, export, packaging and provisioning workflow. Kernel,
driver, device-tree, PL and boot changes still require matched artifacts and their
appropriate installation/activation steps. `scp` only transfers files; it does not
flash a copied WIC or activate a kernel/bitstream. Target account/address and
installation location are supplied by the operator; no automatic deployment occurs.

## Root filesystem and SD image (.wic)

`python3 tools/build.py project` creates the Vivado project and host-built software;
it does not create a Linux filesystem, ARM packages, ESP32 firmware or SD image.
For a software-only image update using the installed matched hardware:

```sh
cd /home/rklinkhammer/workspace/edf-dev
EDF_DMOTOR_SOURCE=../dmotor ./edf --target arty-z7-20 image
EDF_DMOTOR_SOURCE=../dmotor ./edf --target arty-z7-20 export
```

Export writes `artifacts/arty-z7-20/<run>/sdcard.wic.xz` and `sdcard.wic.bmap`
under EDF's configured artifacts root. `xz -dk sdcard.wic.xz` produces an
uncompressed image. Export packages the existing image; it does not rebuild it.
The current bundle fingerprint also includes software/staging inputs. If the
launcher reports `Selected DMOTOR hardware sources differ`, even after a software
edit, use the full matched build rather than editing the saved fingerprint.
For hardware changes or that mismatch, use `EDF_DMOTOR_SOURCE=../dmotor ./edf --target arty-z7-20 build`
for the full hardware/image/export flow. No command here flashes an SD card.

EDF stages only files listed in `software/sources.json`. New CMake sources,
headers and Python tests must be added there, not merely to the working tree.
The acquisition receiver is assigned to `dmotor-runtime` in the authoritative
`software/yocto/dmotor-semantic-tests.bb`, which EDF staging copies into its layer.
Existing exported images are not updated in place. ESP32 profiles still require
their separate [ESP-IDF builds](prepcb_acquisition.md).

## Planned platform: Zybo Z7-20

The Zybo Z7-20 with 1 GiB DDR3L is selected for later platform-sensor integration;
see [D06](decisions.md#d06--planned-zybo-z7-20-sensor-platform-2026-11-07).
Arty Z7-20 remains the current default and matched build/test target. There is no
Zybo build command or deployable Zybo image yet. Migration will require its own
PS/DDR settings, BD/XDC and harness mapping, then matched EDF boot/Linux generation
and target verification. Existing Arty bitstreams/images are not Zybo artifacts.

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

Project recreation now runs the strict interface check before creating output,
so lost UART parameters or unassigned pins fail before synthesis.
After motor/monitor connection edits, check the saved sources with
`python3 tools/check_arty_interface.py`. While pin assignments are incomplete,
`--allow-unassigned` checks wiring but reports missing pins without claiming
release readiness. See [the Arty interface map](arty_monitor_interface.md) for
motor channels, monitor UART and GPIO diagnostics. UART Lite must explicitly
use 115200 baud to match the monitor protocol; its vendor default is 9600.

Open `build/product/vivado/dvrmc.xpr` in Vivado. Edit the source XDC directly;
it is referenced rather than imported. To retain block-design changes, save the
BD, locate its path using the Vivado Tcl command `get_files */design_1.bd`,
then copy that saved file back to
`platforms/arty-z7-20/vivado/design_1.bd` and review the diff. The next build replaces
the generated project. Do not use a GUI project concurrently with its rebuild.
The only product HLS component is `build/vitis_atomic`, configured by
`platforms/vitis/atomic.cfg`. Motor execution is compiled into the PS daemon.

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

## Clean merged-main rebuild

The [matched ESP32 merge record](evidence/esp32_main_merge/inputs.json) identifies
software 0.10.1, controller revision 13 and all artifact hashes. Native software
CTest now includes portable ESP32 acquisition and timing policy tests (29 total);
Yocto runs both under ARM QEMU (10 executable checks total).

With no other build running, move `build/product`, `build/current_ip` and
`build/vitis_atomic` out of the active output paths. Retain the toolchains and
previous release exports. From the EDF checkout, with the explicit source override:

```sh
export EDF_DMOTOR_SOURCE=/home/rklinkhammer/workspace/dmotor
./edf --target arty-z7-20 hardware --force
./edf --target arty-z7-20 --machine board bitbake -c cleansstate \
  xilinx-bootbin u-boot-xlnx device-tree mc:arty-z7-20-cortexa9-fsbl:fsbl-firmware
./edf --target arty-z7-20 --machine linux bitbake -c cleansstate \
  linux-xlnx dmotor-semantic-tests dmotor-access dvrmc-firmware edf-linux-disk-image
./edf --target arty-z7-20 image
./edf --target arty-z7-20 export
```

This rebuilds the product outputs; it retains downloaded dependencies, installed
toolchains and unrelated Yocto dependency sstate. EDF's source override regenerates
local hardware/recipe inputs; it does not advance EDF's Git pin automatically.

For ESP32, use fresh build **and sdkconfig** paths for each row, with
`IDF_CCACHE_ENABLE=0 CCACHE_DISABLE=1`, target `esp32s3`, and the pinned ESP-IDF
container. The [recorded matrix runner](evidence/esp32_main_merge/build_esp32.py)
and [commands/hashes](evidence/esp32_main_merge/esp32_results.json) show exact inputs.
Choose a new output root when repeating the runner; it refuses existing directories.

| Project under `software/esp32/` | Defaults | Purpose |
|---|---|---|
| `diagnostics/bench` | `sdkconfig.defaults` | UART/U1 inhibited diagnostic |
| `build_check` | `sdkconfig.defaults` | Acquisition adapter compile/link only |
| `diagnostics/ina_timing` | `sdkconfig.defaults` | 1 kHz bounded timing diagnostic |
| `diagnostics/ina_timing` | `sdkconfig.probe100.defaults` | Explicit 100 Hz raw-timeout probe |

The timing diagnostic now uses a 16 ms acquisition period and 10 ms INA window.
[Measured INA results and acceptance procedure](ina_bench_timing.md) distinguish
build/host-test success from pending physical budget acceptance. The
1 kHz label is the RTOS tick rate, not the acquisition rate. The Zynq 1 ms motor
loop is unchanged. Builds do not flash devices or qualify hardware timing.
