# Inhibited Linux inspection backend

This is an opt-in UIO bring-up path, **not the motion-capable production backend**.
`dmotor-inspect` identifies DMI1 and reads status/captured telemetry while all
motors are disabled and outputs are externally inhibited. It never enables,
resets, acknowledges, refreshes watchdogs, publishes targets or changes configuration.
Its sole permitted write is SNAPSHOT=1. It is neither `dmotord` nor `dmotor-maint`.

## Access and ownership

`LinuxInspection::open_uio(index)` uses `/sys/class/uio/uioN/maps/map0/{size,offset}`
and the matching `/dev/uioN` character device, not a hardcoded physical address or
`/dev/mem`. It requires the UIO name `dmotor-inspection`, validates device major/minor,
checks DMI1, and refuses attachment when any enabled bit is set. Mapping geometry
and each requested offset are checked. UIO map selection/offset conventions follow
the [Linux UIO documentation](https://www.kernel.org/doc/html/next/driver-api/uio-howto.html).

The backend holds a nonblocking exclusive `flock` on the open node until destruction.
This rejects another cooperating process, including this tool or future service
using the same convention. **It is advisory, not kernel-enforced exclusive open.**
Restrict device permissions to the designated owner; arbitrary UIO clients can
bypass the lock and access registers directly. Do not unlink/recreate device nodes,
fork an owner into unrelated processes, reload FPGA firmware, unbind the driver,
or remove its overlay while a mapping exists.

ARM accesses use aligned volatile 32-bit loads/stores with `dsb sy`; completion
fences also read back DMI1 from the same peripheral. Actual device mapping
attributes, bus ordering and completion still require target validation. Native
x86 execution is allowed only for mapped-file fixtures; real UIO opening is
rejected there. This is not a portable assertion that ordinary memory fences
implement MMIO ordering.

Recoverable open/stat/mmap/validation failures throw `IoError`. An AXI abort or
removed mapping can instead deliver SIGBUS or terminate/hang the process. No signal
handler attempts to resume MMIO execution. Raw UIO cannot meet the complete
production transport error/removal contract. Disabled-state checks also cannot
exclude an unrelated writer racing an access; external inhibition is required.

## Optional board integration (not yet qualified)

The current EDF matched DVRMC overlay has `xlnx,controller-axi-0.18`, not a UIO
binding. No production overlay or image was changed by this increment.

1. Build the board kernel with the optional [inspection.cfg](inspection.cfg).
2. Incorporate [uio-inspection.dtsi](uio-inspection.dtsi) into the **matched full
   DVRMC overlay**, after its controller label definition. Preserve its firmware,
   clock, reset and other device dependencies. Rebuild/validate that overlay with
   EDF; do not apply this fragment alone or reuse a mismatched bitstream.
3. With outputs independently inhibited, load `uio_pdrv_genirq` with
   `of_id=generic-uio` before the FPGA Manager overlay creates the controller.
4. Identify the `uioN` whose sysfs name is `dmotor-inspection`; grant only the
   designated owner access to that node. Keep other hardware clients stopped.
5. Install the separately produced `dmotor-inspect` package and run, for example:

   ```sh
   dmotor-inspect --externally-inhibited 0
   ```

The option acknowledges a physical precondition; it does not drive a HALT GPIO.
The tool performs one bounded inspection and exits. It cannot clear faults or
start motion. It does not provide the HALT-on-exit lifecycle of the future
maintenance application; that application must use the sole-owner service and a
qualified shared-HALT binding.

These integration fragments are provided for board bring-up, not claimed as a
built or board-tested kernel/overlay combination. No automatic boot service,
permission rule, firmware load or motion operation is installed.

## Validation and production recommendation

`dmotor-linux-tests` uses temporary mapped files, including a nonzero map offset,
DMI1 rejection, enabled-state refusal, read/write allowlists, constructor cleanup,
reacquisition and second-owner rejection both in-process and across `fork()`.
EDF's existing recipe also runs it under ARM QEMU user emulation. The fixtures
exercise Linux calls and decoding, not UIO driver behavior, FPGA register side
effects, device-memory attributes, physical inhibition or AXI completion.

For production, prefer a small kernel driver with exclusive-open ownership,
restricted operations and a defined FPGA removal/reset policy, unless UIO's
remaining limitations are explicitly resolved. Kernel mediation alone does not
make a nonresponding AXI slave recoverable; bus timeout/error behavior still needs
qualification. The next software step is sole-owner service/lifecycle integration
and the shared-HALT platform contract, with powered motion gated on those results.
