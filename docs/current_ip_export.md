# Export the current controller components

This export contains the real current blocks, separate from the DMD1 permanently
inhibited diagnostics peripheral. It does not claim that the shared motor engine
and scheduler are connected or that motor hardware is ready to run.

## Build/export

```sh
/opt/vitis/2026.1/Vivado/bin/vivado -mode batch -source platforms/vivado/export_current.tcl
```

Requires the existing generated atomic-mailbox and shared-engine HLS RTL. The
script packages all dependent RTL and generated memory files, creates IP archives,
and runs the current SystemVerilog processor-side driver test in XSim. Timing
package constants are resolved in staged copies for the Vivado packager; the
original source files are unchanged. Re-export after modifying or regenerating RTL.

| Archive | IP VLNV | Contents |
| --- | --- | --- |
| `build/current_ip/mailbox_control.zip` | `dmotor.org:user:mailbox_control:0.18` | Actual HLS atomic mailbox, arbiter, scheduler, target-age protection and PWM |
| `build/current_ip/shared_engine.zip` | `dmotor.org:user:shared_engine:0.18` | Actual four-context HLS motor kernel and its transport |

The extracted IP repositories are also under `build/current_ip/<block>/ip`.
Keep those directories intact when transferring them. ZIP archives can instead
be extracted to repositories on another machine.

## Add to dvrmc

Open the existing `~/workspace/dvrmc/dvrmc.xpr`, then run in its Vivado Tcl console:

```tcl
source /home/rklinkhammer/workspace/dmotor/platforms/vivado/add_current_repo.tcl
```

This appends repositories and preserves the project's existing repository paths.
It does not instantiate blocks, change addresses or overwrite the block design.
The helper has not been executed against the reference project as part of export.
The components then appear in the IP Catalog under UserIP.

The interfaces are intentionally their current interfaces:

- `mailbox_control` has a serialized host valid/ready read/write interface, **not
  AXI**. It needs an AXI adapter before connection to PS GP0/SmartConnect.
- Its normalized engine handshake is **not directly compatible** with the shared
  engine's 35-word transaction record. The scheduler/lifecycle adapter remains
  required, including initialization, warm-up and confirmed target acceptance.
- Both use a real 20 MHz clock and synchronous active-high reset. The reference
  project's 100 MHz AXI clock needs a supported crossing to this domain.
- Configuration, authorization, encoder snapshots, STOP and protection inputs
  remain integration responsibilities. Do not tie prerequisites high to simulate
  readiness on motor hardware. The exported mailbox/PWM block is not permanently
  inhibited like the diagnostics endpoint; physical outputs must stay disconnected
  or externally inhibited during this development integration.

## SystemVerilog processor-side simulation

Open `build/current_ip/mailbox_control/project/mailbox_control.xpr`, choose
**Run Behavioral Simulation**, and run all. Simulation top: `mailbox_control_tb`.
For waveform inspection, add top-level host, frame, result and PWM signals before
running, or restart the simulation after adding them.

The SV driver performs host register reads/writes and publications. The mailbox,
arbitration, scheduler, age timers and PWM are actual RTL, including generated
mailbox HLS RTL. Motor result responses are behavioral. This is a transaction-level
processor model, not ARM instruction execution or a Zynq PS software simulation.

The test covers publication, common PWM release, host response backpressure,
no-batch ticks, cancellation, generation/retirement races, timeout and session
protection. Logs are under:

`build/current_ip/mailbox_control/project/mailbox_control.sim/sim_1/behav/xsim/`

The separately verified shared-engine RTL test remains available through
`tools/shared_engine_rtl.py`; it is not joined to the scheduler in this XSim test.
There is no new full-controller utilization, routed timing or board-bitstream claim.

Vivado 2026.1 successfully packaged both blocks and the XSim host test reported
`mailbox_control PASS`. Evidence: `docs/evidence/current_ip_export/`.
