# Engineering Design Prompt
## DMOTOR — Portable, Autonomous, Single-Motor FPGA Controller
**Revision 0.17 — Shared Compile-Time Ring Capacity**  
**Updated:** 2026-10-11  
**Workspace:** `~/workspace/dmotor`  
**Document:** `DMOTOR_ARCH.md`

**Target platforms:** AMD Zynq-7000 `xc7z020clg400-1` / Vitis HLS and Microchip PolarFire SoC / SmartHLS  
**Default motor:** JGA25-370, 6V, 130 RPM, 46.8:1 gearbox, 11 encoder pulses per channel per motor revolution  
**Target Vivado reference:** `~/workspace/dvrmc/dvrmc.xpr` (Arty Z7-20 preset; see [integration reference](docs/vivado_reference.md))  
**Reference repository:** [rklinkhammer/dvrmc](https://github.com/rklinkhammer/dvrmc)  
**Status:** Architectural specification under review. Sections 22–26 make the protocol and verification contracts concrete. User decisions explicitly marked PENDING are not authorized defaults; the affected implementation must wait for resolution. No synthesis, timing, or physical safety claims are made by this revision.

### 1. Role and objective

Act as a senior FPGA, embedded systems, and motor-control architect experienced in AMD Vitis HLS, Microchip SmartHLS, real-time control systems, AXI4-Lite peripherals, and synthesizable C++.

Design a reusable, parameterized, autonomous FPGA motor controller for a single brushed DC motor with Hall-effect quadrature feedback.

The controller shall accept motion commands from a supervisory processor, independently execute closed-loop velocity and position control, and generate PWM, direction, enable, and braking signals for an external motor driver.

The design shall support both AMD Vitis HLS and Microchip SmartHLS while minimizing vendor-specific code.

The implementation must preserve the following fundamental principles:

1. **One controller instance controls exactly one motor.**
2. Multiple motors are supported by duplicating the same controller instance.
3. The processor performs all multi-motor coordination, including differential-drive calculations.
4. The FPGA performs all time-critical single-motor control operations.
5. Motor control continues autonomously without processor intervention between commands.
6. Hardware HALT notifications can propagate critical conditions between controllers.
7. Motor characteristics and control parameters are configurable.
8. The processor communicates through a stable memory-mapped register interface.
9. Continuous operation shall not depend on processor-issued HLS transactions. An autonomous hardware scheduler may drive internal HLS `ap_start`/`ap_done` handshakes.
10. Simulation, verification, and synthesis evidence are required before hardware deployment.

The architecture should favor simplicity, determinism, and maintainability over unnecessary abstraction.

Decisions incorporated in this revision:

- A hardware scheduler may operate HLS blocks autonomously.
- HALT has the highest operational priority; operating modes and lifecycle states are separate.
- Timing measurement hooks and deadline monitoring are required.
- Commands are queued through an MMIO-accessible descriptor ring; BUSY indicates active command processing, not queue fullness.
- The watchdog has dedicated time-configuration registers and an independent hardware timer.
- The memory map is sized by the design and hardware resources, without an arbitrary 4 KB restriction.
- Encoder conversion preserves fractional counts per output revolution.
- External physical-fault inputs supplement encoder-derived and internal fault detection.

Sections 22–26 define the revision 0.16 baseline and take precedence over exploratory wording in earlier sections. Normative protocol rules are distinguished from proposed numerical acceptance targets and pending user/board decisions. This is not yet a frozen release ABI.

### 2. Project background

Review the existing `dvrmc` repository and its implementation history, together with the local `~/workspace/dvrmc-portable` repository. Develop the new implementation in `~/workspace/dmotor`; do not overwrite either reference repository.

Previous designs explored:

- Zynq-7000 motor controllers using Vitis HLS.
- Quadrature Hall-effect decoding.
- AXI-stream encoder event processing.
- Fixed-point PID controllers.
- PWM generation.
- Acceleration and deceleration limits.
- Position and velocity commands.
- Motor simulation and quadrature signal generation.
- Multi-motor differential-drive control.
- Migration toward PolarFire SoC and SmartHLS.
- Hardware HALT propagation.
- Processor-independent control execution.

The new design shall consolidate these concepts into a simpler, reusable, single-motor architecture.

The existing repository is prior art, not a mandatory architectural template.

Identify which existing components can be reused and which should be redesigned.

Do not preserve unnecessary complexity merely for compatibility with earlier implementations.

### 3. System architecture

The system consists of three layers.

**Supervisory processor:** Receives operator commands, performs vehicle-level kinematics, configures each motor controller, supervises operation, and monitors telemetry and faults.

**FPGA motor controller:** Performs quadrature decoding, velocity estimation, motion profiling, PI/PID control, PWM generation, watchdog supervision, and local fault handling.

**Hardware notification network:** Propagates critical conditions such as HALT between motor controllers independently of the processor. QA/QB remain local feedback inputs; only notification signals connect to the shared HALT network.

Conceptual architecture:

```text
                  SUPERVISORY PROCESSOR
                           |
                     AXI4-Lite MMIO
                           |
             +-------------+-------------+
             |             |             |
             v             v             v
         Motor 0        Motor 1        Motor N
        Registers      Registers      Registers
             |             |             |
             v             v             v
         Motor 0        Motor 1        Motor N
        Controller     Controller     Controller
             |             |             |
             v             v             v
         Motor 0        Motor 1        Motor N
             |             |             |
          QA / QB       QA / QB       QA / QB

        QA/QB feed their respective local controller only.
        Separate HALT_OUT/HALT_IN signals connect each controller
        to the external shared HALT network.
```

Each motor controller shall be independently configurable and operable.

The shared notification network shall be implemented outside the single-motor controller.

### 4. Single-motor controller architecture

Each motor controller shall contain the following functional components:

```text
              +-----------------------------------+
              |     SINGLE MOTOR CONTROLLER       |
              |                                   |
 AXI4-Lite -->| Registers / Command Ring          |
              |       |                           |
              |       v                           |
              | Command / Configuration Bridge    |
              |       |                           |
              |       v                           |
              | Motion Profile Generator          |
              |       |                           |
              |       v                           |
              | PI/PID Controller <----------+    |
              |       |                      |    |
              |       v                      |    |
              | PWM / Direction Generator    |    |
              |       |                      |    |
              |       v                      |    |
              | Motor Driver Outputs         |    |
              |                              |    |
 QA / QB ---->| QCEncoder -------------------+    |
              |       |                           |
              | Velocity Estimator                |
              |                                   |
              | Watchdog / Fault Monitor          |
              | HALT_IN / HALT_OUT                 |
              +-----------------------------------+
```

The implementation shall distinguish between the reusable controller core and platform-specific integration.

The single-motor boundary includes the hardware scheduler, command queue consumer, encoder capture, control algorithms, output generation, independent watchdog, and fault handling. External physical-fault inputs feed the fault monitor and, for configured critical conditions, the output-inhibit path.

Prefer portable C++ HLS for command rings, descriptor validation, register/configuration behavior, lifecycle and motion execution, velocity estimation, profiles, and PI/PID. Maintainability and portability take priority over minimum latency and resource efficiency, within the measured motor-control budgets and device capacity. Evaluate continuously running HLS blocks for encoder decoding and PWM. Retain a minimal RTL boundary for asynchronous-input synchronization, vendor bus/handshake adaptation, independent watchdog/deadline protection, and final output inhibition. No stalled arithmetic or command-engine transaction may prevent HALT or watchdog protection. Existing RTL encoder/PWM/scheduler components remain tested reference implementations until HLS alternatives have equivalent evidence.

The controller core must not contain assumptions about vehicle geometry, steering, wheelbase, or other motors.

### 5. Default motor configuration

The default reference motor is the JGA25-370 brushed DC gearmotor.

| Parameter | Default |
|---|---|
| Motor model | JGA25-370 |
| Rated voltage | 6 V |
| No-load current | 0.1 A |
| No-load output speed | 130 RPM ±10% |
| Rated-load torque | 0.8 kgf·cm (approximately 0.0785 N·m) |
| Rated-load current | 0.5 A |
| Stall torque | 4.4 kgf·cm (approximately 0.431 N·m) |
| Stall current | 2.6 A |
| Reference maximum gearbox output speed | 130 RPM |
| Gear ratio | 46.8:1 |
| Encoder channels | QA and QB |
| Encoder pulses per channel | 11 per motor-shaft revolution |
| Quadrature mode | X4 |
| Counts per motor-shaft revolution | 44 |
| Counts per gearbox output revolution | 2,059.2 |
| Reference maximum encoder count rate | 4,461.6 counts/s |
| Initial PWM frequency | 20 kHz |
| Initial control frequency | 1 kHz |
| Initial fabric clock | 100 MHz |

The current, torque, and no-load speed ratings above were supplied by the user on 2026-10-11 for the reference 6 V motor. Interpret the stated kg.cm torque units as kgf·cm. The exact supplier datasheet is not available, and speed at rated load was not specified. These are supplied design inputs, not independently measured model parameters. Stall ratings are not continuous operating ratings.

The motor parameters shall not be hardcoded into the controller algorithms.

The controller shall support different motor profiles, including motors used on a separate four-motor vehicle platform. The system shall support at most four motors. Optimize computation cost within the existing independent-controller architecture before considering architectural changes; no shared multi-motor arithmetic engine is approved by this requirement.

Motor profiles shall be maintained in processor software and translated into hardware configuration registers.

The controller shall distinguish motor characteristics, controller tuning, and vehicle-level configuration.

### 6. Motor parameterization

Provide a configuration model supporting:

**Physical motor parameters:** Rated voltage, reference maximum speed, gearbox ratio, encoder resolution, encoder polarity, and motor direction polarity.

**Control parameters:** PI/PID gains, maximum velocity, acceleration, deceleration, position tolerance, integral limits, and PWM limits.

**Safety parameters:** Watchdog timeout, encoder-fault thresholds, HALT policy, and permissible output behavior.

**Timing parameters:** PWM frequency, control-loop frequency, and velocity-estimation parameters.

Compile-time parameters shall be limited to characteristics that materially affect hardware structure, such as arithmetic widths, counter widths, FIFO depth, and supported features.

Runtime configuration shall support different motor types without requiring resynthesis wherever practical.

Configuration changes affecting measurement scaling or safety shall be validated and committed atomically while the controller is disabled.

### 7. QCEncoder subsystem

Implement a custom quadrature decoder named `qcencoder`.

The subsystem shall accept asynchronous QA/QB inputs and produce signed position and velocity-related measurements.

Required capabilities include:

- Two-stage or appropriately justified input synchronization.
- X1, X2, and X4 decoding.
- Gray-code transition validation.
- Signed 64-bit position accumulation.
- Direction detection.
- Invalid-transition counting.
- Configurable encoder polarity.
- Position initialization and reset.
- Encoder fault monitoring.

For the JGA25-370, X4 decoding produces 44 counts per motor-shaft revolution.

The gearbox ratio shall be applied separately when calculating output-shaft position and velocity.

The design shall not round the effective 2,059.2 counts per output-shaft revolution to an integer. Store the encoder edge count as an integer; preserve fractional accuracy in the conversion between counts and physical shaft units. For the reference motor, represent the nominal gearbox ratio as 468/10 and the X4 output counts/revolution as 10296/5. Apply configurable calibration without silently truncating the fraction.

The detailed design shall select rational arithmetic or a fixed-point representation with a documented error bound, sufficient intermediate widths, and defined rounding. Position conversion and accumulated motion must not introduce an integer-counts-per-output-revolution assumption. Arithmetic precision does not imply that the nominal gearbox ratio is an exact physical calibration.

The decoder shall operate independently of the 1 kHz control loop.

Avoid AXI-stream interfaces between tightly coupled internal functions unless their use provides a demonstrated benefit.

### 8. Velocity estimation

Implement a configurable velocity estimator that provides useful measurements across the motor's operating range.

Evaluate:

- Fixed-window encoder counting.
- Period measurement between quadrature transitions.
- Hybrid count/period estimation.

At 130 RPM output speed, the default motor produces approximately 4,462 X4 events per second.

A 1 kHz counting window therefore contains only approximately 4.46 events at the reference maximum speed.

The initial architecture should favor a hybrid estimator capable of accurate low-speed measurement and stable high-speed operation.

The implementation shall explicitly handle zero-speed detection, timeouts, reversals, measurement filtering, and invalid encoder transitions.

Counts per gearbox-output revolution shall be configurable and may be fractional. The active conversion remains fixed until a validated configuration commit while disabled. Changes in X1/X2/X4 mode, polarity, or scaling shall establish a defined measurement epoch and reset or reinitialize affected estimator history.

### 9. Motion modes and lifecycle state strategy

Separate the requested motion mode from the controller lifecycle/safety state.

Motion modes:

| Mode | Meaning |
|---|---|
| Velocity | Closed-loop speed target |
| Absolute position | Move to a position in the configured coordinate system |
| Relative position | Move by a displacement relative to measured position at command activation |
| Open-loop | Bounded duty command for commissioning |
| Controlled stop | Decelerate to zero using configured limits |

Lifecycle states (transition contract in Section 22):

| State | Behavior and allowed transition |
|---|---|
| DISABLED | Motion inhibited; configuration commits and explicit enable permitted |
| READY | Explicitly enabled but no active motion; defined non-driving output state |
| RUNNING | Execute an active motion command and autonomous control ticks |
| STOPPING | Execute controlled deceleration; completion enters READY or DISABLED as explicitly requested |
| HALTED | Latched protective stop caused by HALT_IN or a local HALT-class fault; bypass normal command processing |
| FAULT | Latched non-HALT fault; outputs inhibited; requires recovery |

HALT is the highest operational priority. Reset/power-up establishes safe outputs and cannot enable motion while HALT is active. Same-clock arbitration is:

1. HALT_IN or a local HALT-class condition inhibits outputs and establishes HALTED.
2. Other protective faults inhibit outputs and follow the fault/action matrix in Section 22. All protective faults request shared HALT under D07; it is not an unspecified runtime policy.
3. Explicit disable cancels motion and inhibits outputs.
4. Controlled-stop requests take precedence over ordinary queued motion.
5. Normal command activation, completion, configuration acknowledgment, and telemetry updates proceed only if the higher-priority conditions permit them.

If HALT coincides with command activation, the command must not produce an enabled output. Fault capture and telemetry may continue while motion is inhibited. Input release alone never resumes motion.

Provide an out-of-band disable/stop path so a full queue or long-running position move cannot block these requests. A queued controlled-stop command remains available for planned sequences; distinguish it from the immediate stop request.

On HALT, protective fault, or explicit disable, invalidate pending motion and abort the active command. Report the reason and advance a queue epoch so stale commands cannot execute after recovery. Re-enable requires fresh authorization and fresh motion commands. An immediate controlled stop cancels queued motion and advances the queue epoch; a queued controlled stop preserves following descriptors. Neither operation clears a protective latch.

The default velocity controller uses fixed-point PI control. Optional derivative control requires justification. Include saturation, anti-windup, configurable gains, optional feedforward, and deterministic update timing.

Position control uses an outer position loop or motion-profile generator to produce velocity targets. Initially support trapezoidal acceleration and deceleration; jerk limiting is a future extension. Define completion using position tolerance, velocity tolerance, and a settling interval. Define retargeting, overshoot handling, reversal, and unreachable-target timeout behavior.

### 10. PWM and motor-driver interface

The controller shall generate signals suitable for an external brushed DC motor driver.

Logical outputs shall include PWM, direction, enable, and brake.

Required features include configurable PWM frequency, bounded duty cycle, safe direction reversal, reset-safe outputs, and configurable coast/brake behavior.

Motor-driver-specific electrical conventions shall be isolated in a platform adapter.

The implementation shall not assume that PWM, enable, and brake have identical semantics across different H-bridge devices.

The initial prototype shall disable drive and coast on HALT, watchdog expiration, and HLS deadline failure, without active braking. This does not guarantee a stopping distance or position hold. Latch direction while inhibited rather than using reversal to stop. Residual inductive current follows the driver's documented decay path.

The actual motor-driver hardware must be identified before final electrical integration. Reset/power-up pin biases must establish non-driving outputs before FPGA configuration, without depending on software. Verify coast versus short-brake behavior in the selected driver mode.

### 11. Autonomous scheduling and timing measurement

The motor controller operates continuously without processor-issued start commands. One primary fabric clock with clock enables for slower operations is preferred.

A hardware scheduler may autonomously operate HLS blocks using internal `ap_start`, `ap_ready`, `ap_done`, or equivalent SmartHLS handshakes. It shall hold inputs according to the generated interface contract, capture valid results, and prevent overlapping operations unless explicitly supported by the design. The processor does not service these handshakes, and normal operation does not require software-managed auto-restart.

`ap_ctrl_none` remains an optional implementation choice, not a requirement. Removing a handshake is not evidence of one-cycle execution or continuous input sampling. Verify each synthesized block's actual latency and initiation interval on both platforms.

Distinguish these timing domains within the primary clock:

- Fabric-clock operations: input synchronization/capture, quadrature counters, PWM timing, HALT inhibition, watchdog timing, and deadline monitoring.
- Control-tick operations: coherent encoder snapshot, estimator arithmetic, motion profile, and PI/PID computation.
- Processor operations: descriptor publication, configuration commit, telemetry snapshots, heartbeat, and acknowledgment.

The timebase, encoder capture, PWM generation, HALT path, and watchdog shall continue independently of HLS completion and MMIO bus activity. Never derive elapsed time by counting completed HLS calls. Establish a worst-case computation deadline and a defined tick-overrun policy; do not silently drop or accumulate control ticks.

For the initial 100 MHz clock and 1 kHz loop, the nominal control interval is 100,000 fabric clocks. At 20 kHz, the nominal PWM period is 5,000 clocks. Supported frequencies, divider rounding, and update boundaries must be specified. Stage duty changes and apply them at a defined PWM boundary to avoid malformed pulses; HALT output inhibition overrides that boundary.

Required measurement hooks:

| Hook | Required evidence or telemetry |
|---|---|
| Free-running fabric counter | Coherent 64-bit timestamp and declared clock frequency |
| Control tick and computation start/done | Tick count, last/max computation cycles, start lateness, tick overruns, deadline misses |
| Command publication/acceptance/activation/completion | Sequence/epoch identifiers, timestamps, rejection/cancellation reason, queue occupancy and high-water mark |
| Encoder transitions | Valid/invalid counts, last-valid-edge timestamp, measured edge interval, timeout status |
| PWM update | Requested and applied duty, application timestamp, reversal-inhibit status |
| HALT/fault path | Detection timestamp, local/external source, output-inhibit timestamp, last/max response cycles |
| Watchdog | Last authorized refresh timestamp, elapsed/remaining time, expiration count and cause |

Expose coherent diagnostic snapshots through MMIO. Counter widths, wrap/saturation behavior, reset values, units, and explicit clear operations belong in the register specification. A sticky diagnostic clear must not silently clear a protective latch.

Provide optional RTL debug outputs for control tick, HLS busy/start/done, synchronized HALT detection, and final output-inhibit state for ILA/SignalTap-equivalent probes, simulation, or external instrumentation. These hooks must not backpressure the controller or alter functional scheduling.

Verify HALT latency in RTL from both the external input transition and the synchronized detection point to the defined output state. Internal timestamps measure the latter; asynchronous-input sampling uncertainty must be accounted for separately. Include worst-case active computation, full queue, stalled processor bus, reset release, and direction reversal. Document the assumed running clock; detecting loss of that clock requires an independent external mechanism.

### 12. Memory-mapped register interface

Implement a stable, versioned, 32-bit MMIO interface. AXI4-Lite is the preferred processor bus. Keep the register peripheral logically separate from the autonomous controller and HLS arithmetic blocks.

Implement register semantics and command storage in C++ HLS wherever practical. A thin platform adapter must preserve the explicit logical register map, independent AXI address/data handshakes, ordering, response codes, and cancellation/authorization rules; generated HLS control-register layouts must not silently become the public ABI. Both platforms expose the same logical register model. Processor transactions must not be required to advance the autonomous engine.

There is no arbitrary 4 KB limit. Size the aperture, address width, command storage, and completion storage according to the final design and available hardware resources. The earlier illustrative address ranges are not fixed ABI assignments.

Allocate distinct regions for:

- Identity, ABI version, capabilities, aperture size, hardware clock frequency, and reset state.
- Immediate enable/disable/controlled-stop requests and recovery authorization.
- Queue producer/consumer indices, queue epoch, doorbell, occupancy, and descriptor slots.
- Command acceptance, active-command state, rejection, cancellation, and completion records.
- Staged/active physical, estimator, motion, tuning, and timing configuration.
- Dedicated watchdog configuration and refresh registers.
- Coherent telemetry and timing measurement snapshots.
- External-fault configuration, HALT attribution, sticky fault flags, and interrupt masks/status.

Specify every offset, width, signedness, unit, access permission, reset value, and side effect. Define byte-strobe support, alignment rules, unmapped-access responses, acknowledgment behavior, and behavior during reset/HALT. Invalid bus accesses must complete with a defined response rather than hanging the interconnect.

Use a single machine-readable register description to generate the processor header, RTL constants, and documentation. Version changes that alter layout or semantics. The existing dvrmc-portable ABI is reference material, not a required compatibility constraint.

### 13. Queued commands and atomic configuration

Motion commands are queued using a bounded descriptor ring exposed through MMIO, conceptually similar to a DMA submission ring. The initial design uses on-chip descriptor storage and does not require an AXI master that fetches descriptors from processor DDR. DDR-resident DMA chains would be a separate extension with coherency, address-validation, and bus-fault requirements.

Use one logical software producer and one hardware consumer per controller. Multiple software writers must serialize through the driver. Queue depth is a hardware parameter reported through a capability register.

Publication overview (exact ownership/reset contract in Section 23):

1. Software reads consumer progress/available space and reserves only free slots.
2. Software writes complete descriptors, including opcode, flags, sequence number, queue epoch, and motion parameters. Multiword fields are not visible as commands until publication.
3. After the required device-memory ordering barrier and any platform-required posted-write completion, software advances a producer counter/doorbell atomically.
4. Hardware checks the publication against capacity and the active epoch, then treats published descriptors as immutable. Writes to published or active slots must not alter accepted commands; implement ownership enforcement or an equivalent protected capture mechanism.
5. Hardware validates descriptors in FIFO order and captures one into active storage. A slot becomes reusable only after its contents have been captured into protected storage or rejected, as reported by the consumer counter.
6. Acceptance, activation, completion, rejection, and abortion are reported separately using sequence and epoch identifiers.

Use monotonic producer/consumer counters with modulo-depth slot addressing and specified wrap arithmetic. Reject over-capacity or invalid publication without overwriting live entries. Define whether the chosen counter representation permits use of every slot; do not rely on ambiguous equal-index full/empty detection.

BUSY means a command is actively being processed or executed. It does not mean the queue is full, and it does not prevent appending commands when space exists. Report queue occupancy/full, command-processing state, and motor motion state separately. An enabled velocity controller can continue holding a previously applied target even when the command processor is not BUSY.

Command execution semantics make FIFO progression explicit:

- A velocity or open-loop setpoint command completes when its target is applied; autonomous tracking continues until a subsequent command or protective event changes it.
- Absolute/relative position moves complete after reaching the target within configured position/velocity tolerances for the settling interval. Subsequent queued motion waits.
- A relative move is based on the coherent measured position at activation, not at descriptor submission; capture that reference exactly once.
- A dwell/hold-duration command keeps the preceding target active for a specified hardware-timed duration when a timed sequence is required.
- A planned controlled stop completes when its stopping criterion is met.
- A nonterminating hold, if supported, must have explicit cancellation semantics and must not masquerade as an ordinary finite move.

An invalid descriptor produces a rejection completion without execution, then the next descriptor may be processed. Rejection alone does not escalate to a protective fault. Motion timeouts and physical failures follow the fault policy rather than silently advancing a failed sequence.

Provide ordered, bounded completion reporting sufficient to distinguish each command outcome. Use the completion ring and reservation/backpressure rules in Section 23. A slow processor must not stall PWM, encoder capture, HALT, watchdog, or the control loop. Reserve completion capacity before starting commands, stop accepting/starting ordinary commands if needed, and provide sticky out-of-band fault/abort status that cannot be blocked by a full completion ring. Queue invalidation shall identify the canceled epoch/range even if individual completion records cannot all be emitted.

Configuration updates use a separate staged commit with validation and acknowledgment. Every descriptor carries the active configuration generation. A commit requires disabled state, no active command, and an empty or explicitly invalidated motion queue before changes to scaling, safety, or timing are activated. This prevents queued commands from silently changing physical meaning.

HALT, watchdog expiration, protective faults, and immediate disable bypass the ring. Recovery changes the queue epoch and rejects stale publications. Define reset ownership and index initialization so delayed processor writes cannot resurrect pre-reset motion. No queued command may clear HALT or implicitly re-enable the controller.

### 14. Hardware notification and physical-fault inputs

Each controller exposes:

| Signal | Direction | Meaning |
|---|---|---|
| HALT_IN | Input | Highest-priority external protective-stop request |
| HALT_OUT | Output | Local critical condition requesting shared HALT |
| FAULT_OUT | Output | Local latched fault indication |

The controller does not know how many other controllers exist. The shared HALT aggregator is external and may group controllers without changing the core.

```text
Motor 0 HALT_OUT ----+
Motor 1 HALT_OUT ----+
Motor 2 HALT_OUT ----+---- Latched HALT aggregator
Motor 3 HALT_OUT ----+               |
                                    +---- HALT_IN on each participating motor
```

Receiving HALT_IN shall not automatically assert HALT_OUT. Preserve local cause flags and external-HALT status separately, so the initiating fault remains identifiable. Define reset and acknowledgment ordering between the aggregator and controllers; release of the shared latch never itself enables motion.

Add board-adapted physical-fault inputs. The supported set and polarity are capabilities/configuration, with optional unused inputs tied to their defined inactive levels:

| Logical input | Example physical source |
|---|---|
| DRIVER_FAULT_IN | H-bridge nFAULT/status output |
| OVERCURRENT_IN | Current comparator or protection circuit |
| OVERTEMP_IN | Motor/driver thermal switch or comparator |
| SUPPLY_FAULT_IN | Undervoltage/overvoltage monitor |
| LIMIT_POS_IN / LIMIT_NEG_IN | End-of-travel switches |
| ENCODER_FAULT_IN | Encoder supply or external sensor diagnostic |
| AUX_FAULT_IN[N] | Board-specific protective inputs |

These digital inputs do not imply that the FPGA measures analog current, voltage, or temperature. Analog monitoring requires external comparators/ADCs and a defined interface. Consolidated driver-fault signals cannot identify a more specific cause without additional status information.

For each supported input define electrical polarity, synchronization, minimum pulse width or event-capture method, qualification/filter delay, inactive wiring, reset behavior, latching, clear conditions, and fault identifier. Critical inputs route to a bounded-latency output-inhibit path independent of the HLS block and control tick. HALT_IN is mandatory and cannot be masked by normal software configuration.

All qualified protective faults inhibit locally and request shared HALT under D07. Energized escape motion from a travel limit is unsupported in the baseline. Protective configuration changes require disabled state. Defaults must not allow a board integration to silently ignore a required physical protection input.

Continue detecting invalid quadrature transitions, control deadline failures, queue/protocol errors, and watchdog expiration internally. No-transition timeout alone cannot distinguish a stopped motor from a failed encoder; inferred stall/sensor faults must combine explicit effort/motion/time thresholds and available physical feedback. Preserve first-cause and accumulated-cause information, including simultaneous local faults.

### 15. Watchdog, protective behavior, and recovery

The controller implements hardware motion authorization, a command/authorization watchdog, encoder and external-fault monitoring, bounded PWM, direction-reversal protection, reset-safe outputs, HALT, fault latching, and explicit recovery.

The watchdog has dedicated configuration registers for its timeout and declared time units, separate from the control-loop period. Use an independent fabric-clock timer that runs regardless of HLS progress, queue activity, or MMIO traffic. Define range checks, tick conversion/rounding, elapsed-counter width, and wrap/saturation behavior. Permit safety configuration updates only while disabled.

Provide an explicit refresh register/protocol. Ordinary MMIO reads, command queue occupancy, and unrelated writes do not implicitly refresh motion authorization. Define a refresh sequence/epoch rule to reject stale refreshes after recovery and the exact priority of a refresh coincident with expiration. A future finite-trajectory authorization policy would need explicit bounds; queued work alone is not indefinite authorization.

At expiration, inhibit outputs immediately, latch WATCHDOG, abort active motion, and invalidate queued motion. A late refresh cannot resume execution. Shared-HALT escalation and the authorization lifecycle are controlled by decisions D01 and D02 in Section 22.

HALT is the highest operational priority and bypasses command processing, PID timing, and PWM update scheduling. The final output adapter shall define safe logical and electrical states for PWM, direction, enable, and brake. Coast versus brake depends on the actual driver and application; do not assume all-high/all-low is universally safe. Measure and verify the maximum response in fabric cycles under documented synchronization assumptions.

Recovery requires removal of the underlying condition, explicit acknowledgment, coordinated release of the shared HALT latch, and explicit re-enablement. Acknowledgment alone does not enable outputs. Separate acknowledgment of a local cause from clearing the external protective-state latch so aggregator feedback does not make recovery impossible. No motor may restart while any effective HALT or mandatory fault remains active.

The FPGA HALT network is not a certified emergency-stop system. Board-level protection for clock/power loss and motor-driver electrical faults is outside the guarantees of a single-clock FPGA controller and must be addressed by the hardware integration.

### 16. Telemetry, diagnostics, and interrupts

Provide coherent telemetry including signed position, estimated velocity, target velocity/position, requested/applied duty, direction, operating mode, lifecycle state, encoder errors, local/external HALT attribution, physical/internal fault flags, watchdog state, active command sequence, last accepted/completed command, queue state, and configuration generation.

Include the measurement hooks in Section 11. Snapshot multiword values atomically so processor reads cannot tear a 64-bit position or timestamp. Define snapshot sequence and capture time; use explicit capture or a documented consistency protocol.

Interrupts support faults, HALT, command rejection, completion availability, and queue conditions as required. Use documented masks, sticky status, and acknowledgment ordering. Normal telemetry does not require an interrupt on every control tick. Clearing an interrupt does not clear the underlying fault or authorize motion.

### 17. HLS portability

The implementation shall separate portable algorithms from platform-specific interfaces.

Proposed organization:

```text
dmotor/
├── core/
│   ├── qcencoder.hpp
│   ├── velocity_estimator.hpp
│   ├── pid.hpp
│   ├── motion_profile.hpp
│   ├── motor_controller.hpp
│   ├── motor_types.hpp
│   └── safety.hpp
├── rtl/
│   ├── register_interface/
│   ├── command_bridge/
│   ├── command_queue/
│   ├── scheduler/
│   ├── encoder_capture/
│   ├── pwm/
│   ├── watchdog/
│   ├── diagnostics/
│   ├── halt_aggregator/
│   └── safety_adapter/
├── platform/
│   ├── vitis/
│   └── smarthls/
├── simulation/
│   ├── motor_model.hpp
│   └── encoder_model.hpp
├── tests/
│   ├── unit/
│   ├── integration/
│   └── regression/
├── software/
│   ├── include/
│   ├── driver/
│   └── motor_profiles/
├── examples/
│   ├── zynq7000/
│   └── polarfire_soc/
└── docs/
```

Use portable C++ for control algorithms.

Isolate vendor-specific pragmas, fixed-point implementations, and interface constructs.

Avoid a large compatibility framework unless required by demonstrated toolchain differences.

Use the HLS-first partition in Section 4. Keep synchronization and protective output inhibition independent of HLS scheduling. Any HLS pin-processing block must run continuously and meet the capture/update envelope independently of slower command and arithmetic engines.

The design shall not claim portability until both toolchains have compiled and synthesized the relevant components.

### 18. Motor simulation

Develop a behavioral brushed DC motor model that accepts PWM, direction, enable, and brake signals.

The model shall produce realistic motor motion and Hall quadrature outputs.

Support configurable:

- Motor electrical parameters.
- Rotor inertia.
- Gearbox ratio.
- Friction.
- Load torque.
- Encoder resolution.
- Sensor polarity.
- Supply voltage.

The default model shall use the JGA25-370 configuration.

Unknown physical parameters shall be documented as assumptions.

The simulator shall support fault injection, including missing encoder transitions, invalid quadrature sequences, excessive load, stalled motion, and processor watchdog expiration.

The motor simulator is not required to be synthesizable.

### 19. Verification

Develop independent unit tests for quadrature decoding, fractional scaling, velocity estimation, PI/PID arithmetic, motion profiling, PWM generation, command processing, configuration commits, and fault handling.

Add bus-level/RTL tests for independent AXI address/data handshakes, response backpressure, partial/misaligned accesses, ring ownership, wraparound, over-publication, immutable committed descriptors, full completion storage, reset/epoch races, and coherent telemetry. Verify that MMIO stalls cannot block safety or periodic control.

Exercise HALT simultaneous with command activation and PWM update, every configured external physical-fault input, watchdog expiration with queued work, deliberately stalled HLS arithmetic, computation deadline overruns, and explicit recovery with stale commands present. Correlate internal timing counters with independent RTL testbench observations.

Develop closed-loop integration tests using the motor simulator.

Required scenarios include forward/reverse velocity tracking, position moves, acceleration and deceleration, direction changes, load disturbances, zero-speed detection, encoder faults, HALT propagation, watchdog expiration, and recovery.

Instantiate two and four independent controller instances in simulation.

Verify that a critical fault in one controller causes the configured shared HALT response without falsely attributing the original fault to other controllers.

Measure HALT response latency.

Validate the common C++ algorithms with conventional host tests, then verify their synthesizability and timing behavior independently in Vitis HLS and SmartHLS.

### 20. Proposed implementation phases and acceptance evidence

Define the relevant provisional protocol and test criteria before starting each dependent implementation step; keep numerical integration and vehicle requirements explicitly pending where applicable. Maintain the phases below as evidence gates.

1. **Establish the development contract and test plan.** Use the approved states, command semantics, ring ownership, and shared fault policy. Draft the versioned register description and algorithm numeric profile; test against Section 24 targets. Defer physical adapter validation to integration and vehicle tolerance to final-system measurements. These do not block independent core implementation. Freeze the release MMIO contract after applicable implementation tests pass, not before development starts.
2. **Prove autonomous infrastructure on both vendors.** Implement single-motor MMIO, a descriptor/completion ring, hardware scheduler, independent watchdog, measurement hooks, and safe output inhibition around a minimal arithmetic block. Verify internal HLS handshakes and continued safety operation when that block does not complete.
3. **Implement physical I/O timing.** Add synchronized/qualified QA/QB capture, decoder, PWM, reversal sequencing, external fault inputs, and shared HALT aggregator. Verify waveforms, fault priority, and latency before closed-loop operation.
4. **Implement velocity control.** Integrate hybrid estimation, PI, optional feedforward, calibration, and acceleration/deceleration limits. Validate with the motor model, queue-driven commands, and fault injection.
5. **Implement position and sequence execution.** Add absolute/relative moves, settling/completion rules, dwell commands, stop/cancel behavior, and configuration-generation handling.
6. **Verify two/four independent instances.** Test queued command traffic, processor stalls, ring wrap/full conditions, telemetry consistency, watchdog expiration, HALT attribution and recovery, and resource scaling.
7. **Integrate and validate on hardware.** Package vendor IP, provide board wrappers and software, check timing/resource reports, validate Linux device discovery and load/unload sequencing where applicable, and perform controlled physical motor testing.

Each phase records host-test, RTL simulation, vendor synthesis, and timing evidence appropriate to the change. A passing C++ model is not proof of cycle timing, synchronization correctness, or electrical safety.

### 21. Required design deliverables

Produce an architecture and state-transition specification, generated register/interface definitions, command-ring protocol, timing/measurement plan, numeric range analysis, reusable core and RTL blocks, platform adapters, host driver and motor profiles, simulation models, and automated tests.

Document reuse decisions from the reference repositories. Expected candidates include PI/PID arithmetic, calibration, host motor-profile conversion, and the virtual motor model. Redesign the previous two-motor register engine and software-transaction-driven top as needed; keep vehicle-level differential-drive calculations in processor software.

Apply the existing dvrmc-portable coding-standard principles to the new workspace: clear names and units, explicit control flow, reproducible formatting, documented arithmetic widths and narrowing, and isolated vendor directives. Add RTL conventions for reset, clock enables, synchronization attributes, handshake ownership, and assertions.

Report tool versions, measured latency/throughput/resource use, which configurations passed each verification level, unresolved risks, and board-specific work. Do not claim cross-vendor portability or hardware readiness without the corresponding evidence.


### 22. Fault actions, state transitions, and authorization

#### 22.1 Decision register

| ID | Decision | Status |
|---|---|---|
| D01 | Watchdog expiration and HLS deadline failure inhibit locally and assert shared HALT_OUT | APPROVED by user, 2026-10-11 |
| D02 | Watchdog arms on explicit enable, requires refresh in READY/RUNNING/STOPPING, and expiration wins a same-cycle refresh | APPROVED by user, 2026-10-11 |
| D03a | Initial prototype HALT behavior: disable drive and coast; no active braking | APPROVED by user, 2026-10-11 |
| D03b | Electrical adapter, current/thermal limits, and reset pin biases for the selected board | PENDING engineering and validation; board selection resolved by D03c |
| D03c | Adopt Pololu DRV8263H Single Brushed DC Motor Driver Carrier #5238 | APPROVED by user, 2026-10-11; exact received board revision must be recorded |
| D04 | Minimum supported nonzero steady-state output-shaft speed: 1 RPM magnitude in both directions | APPROVED by user, 2026-10-11; loaded performance requires validation |
| D05 | Zero commanded drive after position completion; no active position holding | APPROVED by user, 2026-10-11 |
| D06 | Determine position tolerance through final-system measurement; system controller owns external localization and compensation | APPROVED measurement approach, 2026-10-11; numerical tolerance remains pending |
| D07 | Every qualified protective fault inhibits locally and asserts HALT_OUT to stop all motors | APPROVED by user, 2026-10-11 |
| D08 | Queue semantics and immediate controlled-stop cancellation/recovery are confirmed; every enabled controller requires continued watchdog refresh | APPROVED by user, 2026-10-11 |
| D09 | Algorithm design defines control parameters; electrical adapter is completed during integration; proposed targets guide verification and may change with evidence | APPROVED by user, 2026-10-11 |
| D10 | MMIO remains provisional during implementation/testing; freeze after required tests pass, allowing new registers for findings | APPROVED by user, 2026-10-11 |
| D11 | Prefer C++ HLS wherever practical, accepting reduced efficiency within verified budgets; retain an independent protection boundary | APPROVED implementation direction, 2026-10-11 |

D01, D02, D03a, D03c, D04, D05, and D07–D10 are approved requirements. D06 approves the measurement approach, not a numerical tolerance. D03b remains pending electrical engineering and validation; board selection is resolved. The baseline digital output intent is `motion_authorized=0`, zero requested drive, and no active braking. The selected adapter must translate this into its documented disable/coast state; zero PWM must not be assumed to mean coast in every driver mode. The interface carries `inhibit` separately from commanded duty so an arithmetic result cannot override it.

#### 22.2 Fault/action matrix

Actions are fixed in this baseline except the explicitly pending decisions. All qualified protective faults inhibit locally, latch their cause, set FAULT_OUT, assert HALT_OUT, and enter HALTED; merely receiving external HALT sets a separate external-HALT latch and does not create a local fault or assert HALT_OUT.

| Event | Immediate response/state | Latch / HALT_OUT | Active command and queued work | Recovery |
|---|---|---|---|---|
| HALT_IN | Inhibit; HALTED | External latch; no automatic HALT_OUT | Abort active; invalidate pending epoch | External HALT released, acknowledge, explicit enable |
| Overcurrent, overtemperature, supply fault, driver fault | Inhibit; HALTED | Local cause latch; HALT_OUT asserted | Abort/invalidate | Physical cause inactive, local acknowledge, aggregator release, external-latch acknowledge, enable |
| Encoder hardware fault, excessive invalid transitions, configured stall criterion | Inhibit; HALTED | Local cause latch; HALT_OUT asserted | Abort/invalidate | Cause corrected, local acknowledge, aggregator release, external-latch acknowledge, enable |
| Travel-limit input | Inhibit; HALTED | Local cause latch; HALT_OUT asserted | Abort/invalidate | Limit released, local acknowledge, aggregator release, external-latch acknowledge, enable; no energized escape motion |
| Watchdog expiration | Inhibit; HALTED | WATCHDOG latch; HALT_OUT asserted | Abort/invalidate | Fresh authorization and fault recovery per D02 |
| HLS deadline failure or tick overrun | Inhibit; HALTED | DEADLINE latch; HALT_OUT asserted | Abort/invalidate; discard late result | Reset arithmetic scheduler, acknowledge, enable |
| Position/stop timeout, arithmetic range fault | Inhibit; HALTED | Local cause latch; HALT_OUT asserted | Abort/invalidate | Correct command/configuration, local acknowledge, aggregator release, external-latch acknowledge, enable |
| Invalid descriptor | No new motion; retain existing valid control target | Rejection record and sticky diagnostic; no FAULT_OUT/HALT_OUT | Reject this descriptor; remaining queue preserved | Software may submit corrected work |
| MMIO ownership violation / invalid publication | Reject offending transaction; no motion effect | Sticky protocol diagnostic; no HALT_OUT | Published work unchanged | Driver correction; quiesce/reinitialize if ownership is uncertain |
| Queue/completion full | Backpressure new command processing; existing target still controlled | Status only | No overwrite, no silent drop | Software drains completions / waits for capacity |
| Immediate disable | Inhibit; DISABLED unless HALT/fault already latched | Preserve existing latches | Abort/invalidate | Explicit enable after all other conditions satisfied |
| Immediate controlled stop | STOPPING unless protective state has priority | No new protective latch | Abort active command as superseded; cancel queued motion | Finish at READY; watchdog remains subject to D02 |
| Reset | Inhibit; DISABLED, or HALTED when external HALT is asserted | Reset domains defined below | Abandon ring session | Reinitialize, validate configuration, explicit enable |

AUX inputs are absent by default. Each enabled AUX input must be assigned one of the existing protective classes in the board integration specification. Unspecified AUX policy is not accepted. Physical input qualification/polarity may be configured only while disabled; mandatory HALT cannot be masked. Every enabled AUX protective class also requests shared HALT. Diagnostic-only events (descriptor rejection, rejected MMIO transactions, and queue backpressure) remain nonprotective as specified in the matrix; they do not by themselves assert shared HALT.

Hardware captures all simultaneous local causes and their timestamps; a first-cause identifier uses the lowest assigned fault bit as the deterministic tie-break. That reporting order does not change HALT priority.

#### 22.3 State transition rules

- Power-up and peripheral reset start with motion inhibited, no valid configuration, and queue admission closed.
- DISABLED -> READY requires valid active configuration, an initialized/quiesced queue session, no asserted mandatory fault input, no remaining protective latch, and explicit ENABLE. D02 controls watchdog arming on this transition.
- READY -> RUNNING occurs only when a valid motion command activates at a control boundary. Publishing a descriptor alone cannot enable motion.
- A completed velocity setpoint leaves the lifecycle RUNNING while target tracking continues, even though BUSY becomes zero. Completion of a position move enters READY with zero commanded drive; the baseline does not provide active position holding after completion.
- STOPPING -> READY requires the configured speed threshold for the configured settling duration. Deadline/timeout causes a fault; reaching READY does not imply the rotor is mechanically locked.
- HALT/fault transitions occur independently of control ticks. No command or ENABLE can override a protective latch.
- All configuration changes in the baseline require DISABLED, no active command, an empty submission ring, and queue admission closed. Successful commit increments CONFIG_GENERATION. There is no live-retuning exception in revision 0.9.
- The scheduler ignores HLS results belonging to an invalidated command/configuration epoch. Recovery must reset or drain the arithmetic engine before admitting new computation.

Local-cause acknowledgment may release HALT_OUT after the underlying local source is inactive even while HALT_IN remains asserted. It does not clear the external latch or permit motion. Software then releases the external aggregator latch, waits until HALT_IN is inactive, acknowledges the external latch, and explicitly enables. This ordering breaks the recovery dependency cycle without automatically restarting a motor.

A peripheral reset may clear local records, but cannot clear the external aggregator's latched HALT. Resetting all protection domains together is a board-level operation requiring inhibited outputs. Software reset, bus reset, and fabric reset must be enumerated in the board wrapper; none may inadvertently reset the shared aggregator during local recovery.

D07 requires the external aggregator to distribute a latched HALT to every participating motor. Test each controller as the initiating source. Only the originating controllers latch local causes; receiving HALT_IN does not echo HALT_OUT. Shared stop means drive inhibition/coasting, not simultaneous mechanical standstill. The FAULT state remains reserved; baseline protective transitions now enter HALTED.

#### 22.4 Approved watchdog authorization lifecycle

D02 approves arming on enable, refresh throughout enabled states, and expiration priority. The lifecycle below is normative. Numerical timeout ranges remain initial engineering-profile choices subject to validation:

- Successful ENABLE atomically arms the watchdog and records the current fabric timestamp. A refresh written while disabled does not pre-authorize a later enable.
- Watchdog remains armed in READY, RUNNING, and STOPPING, regardless of BUSY or queue occupancy. It disarms in DISABLED, FAULT, and HALTED after inhibition is established.
- Timeout is a positive integer number of microseconds, converted to fabric cycles using ceiling division; zero is rejected. Range is 1,000..10,000,000 us in the proposed initial profile. Configuration reset value is 0 (invalid), requiring an explicit commit before enable.
- Expiration occurs when elapsed cycles are greater than or equal to the configured threshold. Expiration wins over refresh or ENABLE on that clock.
- A refresh uses the current session token and the next monotonic refresh sequence, serialized by the driver. Repeated/stale sequence values and writes from an old session are rejected. At enable the expected sequence starts at one; session reinitialization precedes wrap/reuse.
- Software's normal refresh period should be at most one third of the timeout. This is a scheduling recommendation; hardware accepts a correctly sequenced refresh at any earlier, nonexpired clock.
- Dedicated timeout, elapsed-time snapshot, expected refresh sequence, and expiration-status registers exist independently of control-loop timing registers.

The user’s watchdog acknowledgment requirement means a valid periodic authorization refresh for every enabled controller, including while queues are full, during dwell, and while stopping. It is distinct from acknowledgment of an already-latched watchdog fault. Expiration of any participating controller requests shared HALT; refreshing another controller cannot authorize it. Queued motion does not exempt the controller from refresh. Bounded-trajectory authorization without refresh is not part of the approved baseline; adding it requires a separate design decision.

### 23. Submission ring, completions, and MMIO contract

#### 23.1 Storage, ownership, and counters

The initial hardware profile uses 4 submission slots of 64 bytes and 4 completion slots of 32 bytes. Both depths derive from `dmotor::ring_depth` in `include/dmotor/ring_config.hpp`; supported values are powers of two from 2 through 64. Regenerate the ABI artifacts and rebuild host code and HLS kernels after changing this constant. Depth is a compile-time configuration, not a runtime setting. Aperture size is discovered from capabilities and may grow with supported features. No descriptor resides in processor DDR in the baseline.

Use 32-bit unsigned monotonic counters with modular subtraction, queue depths smaller than 2^31, and power-of-two slot indexing. `occupancy = producer - consumer` modulo 2^32 is valid only in 0..depth. All depth slots are usable. Producer counters are hardware-owned; software publishes a requested advance using a validated doorbell. Invalid jumps never modify live indices. Use the same rules for completion acknowledgment.

Each slot follows FREE -> SOFTWARE_STAGING -> PUBLISHED -> CAPTURED -> FREE. Software can write only free/unpublished slots while admission is open. Publication occurs on a successful doorbell write after all descriptor writes have completed. Hardware locks all newly published slots on that clock. Hardware validates/copies a descriptor to dedicated active storage before advancing the consumer counter, which releases the slot. No active command reads mutable staging memory.

A write to a published slot returns SLVERR and has no side effect. A bus write accepted before publication must be fully committed before the doorbell takes effect. A descriptor rejected during validation consumes a completion reservation and releases its slot only after a complete rejection record has been deposited.

There is one software writer (or a driver-held lock across the whole publication sequence) and at most one active motion command. Completion capacity is reserved before descriptor capture or execution. Existing control, PWM, encoder capture, and safety continue when capacity is unavailable. Queue processing does not reserve scarce capacity from the safety path.

#### 23.2 Descriptor wire format

Each descriptor contains sixteen little-endian 32-bit words. Reserved fields must be zero. Position and duration pairs are low word first; descriptor immutability makes their capture atomic. The publication doorbell is separate from the descriptor body.

| Word | Field | Meaning |
|---|---|---|
| 0 | OPCODE | 1 velocity, 2 absolute position, 3 relative position, 4 open-loop duty, 5 dwell, 6 controlled stop |
| 1 | FLAGS | Zero in initial ABI; unknown bits reject |
| 2 | SEQUENCE | Software correlation number; must equal producer position for this slot in the current epoch |
| 3 | QUEUE_EPOCH | Must match current hardware epoch |
| 4 | CONFIG_GENERATION | Must match active configuration |
| 5 | TARGET_VELOCITY_MRPM | Signed output-shaft milli-RPM; velocity target or positive position-move speed cap |
| 6–7 | POSITION | Signed two's-complement integer encoder counts in the active decoding mode; target/displacement by opcode |
| 8 | DUTY_Q16 | Signed normalized duty in -65536..65536 for open-loop only |
| 9–10 | DURATION_US | Unsigned duration for dwell; zero rejected for dwell |
| 11 | TIMEOUT_US | Positive execution timeout for position/controlled-stop commands; independent of authorization timeout |
| 12–15 | RESERVED | Must be zero |

Unused opcode-specific fields must be zero. Unsupported opcodes, invalid signs/ranges, sequence mismatch, or generation mismatch return rejection codes. Relative position addition is checked for signed overflow before activation. Position units are explicitly encoder counts, while fractional gear conversion occurs in host conversion and estimator arithmetic; user-facing revolutions need not be integral. Cumulative relative conversion must retain the fractional residual in software rather than repeatedly rounding each requested displacement independently.

Velocity/open-loop commands complete when the new target is installed at a control boundary; this does not claim the motor has reached that target. Position and controlled-stop commands complete on settling. Dwell preserves the preceding target and finishes after the specified hardware-timed interval; if it follows a completed position move, that target is zero drive. Settle thresholds and timeout rules belong to the validated configuration. No nonterminating queued hold opcode is provided initially.

Completion records contain eight words: sequence, epoch, outcome, detail/fault code, activation timestamp low/high, terminal timestamp low/high. Timestamps are fabric cycles. Outcome encodings are 0 completed, 1 rejected, 2 aborted. A rejected-before-activation record has zero activation timestamp. Producer publication of a completion occurs only after all words are stable; they remain immutable until software acknowledges them.

No new command starts when the completion ring is full. Protective events use an independent sticky cancellation record containing canceled epoch, active sequence-valid/sequence, pending sequence interval, fault cause, and timestamp. That record is frozen until acknowledged; further events accumulate in sticky fault bits/counters without overwriting it. Admission remains closed until this record is acknowledged, so a second cancellation cannot lose a newly admitted sequence range. Queued cancellations are represented by this range rather than requiring one completion slot per canceled command. A reserved completion slot may still report the aborted active command; duplicates are correlated by epoch/sequence.

#### 23.3 Reset, stale writes, and session reopening

An epoch in a descriptor does not protect its body against delayed writes. Admission therefore closes immediately on reset, HALT, fault, disable, or immediate controlled stop. Closing admission rejects subsequent descriptor writes and doorbells; it does not block status reads, completion draining, STOP, or safety handling.

The host must serialize all writers, prevent new submissions, and drain already-issued writes through the platform's device-memory barriers, response completion, and same-peripheral readback before requesting QUIESCE. The AXI adapter must complete or discard partially accepted transactions under a documented reset protocol; AW without W is not a completed descriptor write. A QUIESCED acknowledgment is returned only once every transaction accepted by that adapter has been accounted for. An adapter cannot prove that an upstream interconnect has no delayed writes: the driver/interconnect drain contract is mandatory.

After quiescence and cancellation acknowledgment, software initializes counters/storage, acquires a new hardware queue epoch/session token, writes the configuration if needed, and explicitly reopens admission. Reopen and enable are different operations. The queue may be filled while DISABLED after configuration has been committed, but no command executes until ENABLE.

For cold/fabric reset where the epoch register itself resets, stale transactions cannot be distinguished by token alone. The board integration must reset/drain or isolate the upstream bus before reopening. Epoch/token wrap likewise requires a fully drained session; automatic wrap into a live old generation is prohibited. During ordinary cancellation, increment epoch before reopening. No protection against arbitrary stale writes is claimed without this platform contract.

#### 23.4 MMIO rules and ABI freeze gate

Use little-endian, aligned 32-bit accesses. Initial implementation supports full-word writes only: partial WSTRB, misalignment, reserved writes, and writes to read-only locations return SLVERR with no state change. Reserved reads return zero. AXI read/write channels may backpressure but must not depend on HLS completion to respond. A bounded local bus response is required for complete accepted requests; upstream response backpressure is outside that latency measurement.

Special action registers are write-one command strobes and read as zero. Ordinary RW registers are staging only. Protective status clear is not ordinary W1C: acknowledgment must validate that the associated live source is inactive. Interrupt W1C affects interrupt bookkeeping only; a new same-clock event wins over clear. Read operations have no destructive side effects. Telemetry and diagnostic snapshots are explicit transactions with a snapshot sequence and timestamp.

Publish sequence: descriptor full-word writes -> platform device-write ordering/completion -> epoch/session staging -> producer-advance doorbell -> read back accepted producer/status. The epoch/session and producer must be checked together at the doorbell. Dedicated immediate requests have their own validation and cannot be blocked by queue fullness.

The machine-readable ABI must enumerate the regions in Section 12, every register field/reset/side effect, descriptor and completion layouts above, and capabilities. The current baseline specifies protocol semantics and wire record sizes; it does not invent unreviewed offsets for still-pending fault/electrical policies. A versioned provisional machine-readable map and matching generated RTL/C definitions must accompany implementation; a stable ABI is not a prerequisite to starting it. Add or revise registers as tests expose missing capabilities, updating generated artifacts and tests together. Report development schema/version and capabilities so mismatched software is rejected. Freeze the release MMIO contract only after the applicable protocol, RTL, generated-interface consistency, and integration tests pass and software-visible numeric/electrical requirements are resolved. Vehicle calibration values may remain configurable without changing their frozen formats. After freeze, additions require explicit ABI/version and compatibility review; never silently repurpose existing offsets. Do not label the provisional map ABI 1.0.

### 24. Timing, estimator, and numeric acceptance profile

These are proposed engineering targets for the initial 100 MHz/1 kHz/20 kHz profile, not measured capabilities or physical stopping guarantees. Validate or revise them through synthesis and RTL tests before freezing the profile. The approved minimum nonzero steady-state output-shaft speed is 1 RPM magnitude in both directions. This is an operating requirement, not measured performance; transitions through lower speeds during startup, stopping, or reversal are permitted. Board-specific electrical response remains subject to validation.

| Requirement | Initial proposed target / method |
|---|---|
| Synchronized HALT detection -> logical output inhibit | At most 2 fabric clocks; independent of HLS, bus stalls, and PWM phase |
| External HALT -> detection | Two synchronizer stages, no configurable debounce on HALT; require assertion held until acknowledged |
| External HALT -> logical inhibit | At most 5 clocks (50 ns at 100 MHz) in digital RTL tests, including arbitrary input phase; metastability reliability requires separate CDC analysis and is not a deterministic analog bound |
| Fault qualification | Board-specific for each physical input; add its documented synchronization/filter delay to the 2-clock internal inhibit target |
| HLS control computation | Start within 1 clock of scheduled tick when idle; complete before 80,000 clocks from tick, leaving 20,000 clocks of margin |
| Missed deadline | Fault and inhibit on deadline expiration; completion arriving on that boundary loses; never apply late/stale duty |
| New duty application | First PWM boundary after valid result, within one configured PWM period; inhibition bypasses this rule |
| Local MMIO response | HLS-first proposed budget: at most 2,048 clocks (20.48 us at 100 MHz) from acceptance of a complete request to response VALID, excluding reset and master response backpressure. The register engine and transport are bounded independently of motor arithmetic; see docs/axi_peripheral.md for measured evidence and arbitration assumptions |
| Encoder capture envelope | Demonstrate at least 100,000 valid X4 transitions/s with each state stable for at least 1,000 fabric clocks at 100 MHz; filtering must fit the declared envelope |
| Timing counters | 64-bit modulo fabric timestamps; compare intervals only within half the wrap range; sticky event counters saturate |

Continuous HALT input must stay asserted until the protective latch is observed/acknowledged. Pulse inputs need a separately verified asynchronous event latch or external pulse stretching; a two-flop synchronizer is not a guarantee of capturing arbitrary narrow pulses. The shared-HALT test measures the complete source-controller -> aggregator -> destination path separately; the local 5-clock target must not be reported as the shared-network bound.

The FPGA pin-to-driver response budget remains blocked by D03b; disable/coast is now the approved initial stop mode. Measure disabled/brake pin levels, external driver delay, and mechanical stopping separately. No 50 ns mechanical-stop claim is implied.

#### 24.1 Measurement and estimator acceptance

Reference-motor tests use the stated 10296/5 nominal X4 counts/output revolution. Validate the approved 1 RPM minimum with the tracked-vehicle load as well as the ideal encoder test; estimator accuracy alone does not establish closed-loop capability. Proposed steady-state estimator sweep: both directions at 1, 5, 30, and 130 RPM. With ideal encoder timing and after initialization/filter settling, require relative speed error <= 2% over the assessment window; test quantization and declared filter lag separately from physical model uncertainty. This is an estimator requirement, not a closed-loop tracking or physical calibration claim.

Use a reference zero-speed timeout of 100 ms, configurable while disabled. At 1 RPM the nominal event interval is approximately 29.14 ms. At lower speeds a 100 ms timeout can indicate zero between genuine edges; sustained operation below 1 RPM is outside the initial supported range and would require a larger timeout or a revised accuracy/response requirement. Report sample age and validity; do not claim fresh speed information every 1 ms when no edge has occurred.

On reversal, discard an interval spanning the direction change and acquire new-direction intervals before marking the period estimate valid. Invalid transitions increment diagnostics and never contribute signed position or a valid period interval. Before the first usable measurement, telemetry marks velocity invalid rather than implying a trustworthy zero measurement. Zero timeout produces a valid timeout-qualified zero with distinct status. Require a fresh estimation epoch after reset, position initialization, polarity/scaling change, or decoding-mode change.

For deterministic tests, characterize settling time and phase lag at each sweep point and across estimator crossover. The crossover/hysteresis and filter coefficients remain algorithm-design outputs subject to these requirements; they must not add unbounded history dependence. Missing-edge, stuck-sensor, intermittent-edge, and load-stall tests must distinguish measurable input behavior from inferred physical causes.

#### 24.2 Numeric representation and verification

- Position is signed 64-bit encoder counts. Saturate on overflow and latch ARITHMETIC_RANGE; do not wrap commanded or accumulated physical position silently.
- Public velocity and acceleration are signed milli-RPM and positive milli-RPM/second respectively. Duty uses signed int32 scaled by 65536 with magnitude bounded to one. Timestamps/durations are unsigned with explicit units.
- Store encoder PPR, gearbox numerator/denominator, and calibration numerator/denominator as positive integers. Reject zero denominators. Cross-reduce rational factors before bounded multiplication. Preserve 468/10 and 10296/5 exactly for the default nominal profile.
- Proposed initial software-visible range: encoder PPR 1..65,535; gearbox/calibration components 1..1,000,000; speed limit up to 1,000,000 milli-RPM; acceleration/deceleration up to 10,000,000 milli-RPM/s. Individual fields in range do not imply a valid combination: reject combinations exceeding capture rate, arithmetic width, or timing envelopes.
- Derive intermediate widths from supported combinations before coding arithmetic. Use widened products and a checked reference calculation; there is no permission to silently truncate merely because each input fits 32 bits. Wide arithmetic resource cost must be measured on each FPGA.
- Specify gain units and representation per term before ABI freeze. The existing dvrmc-portable Q16 gain format is a reuse candidate, not automatically the new ABI. Choose widths only after the range analysis; provide an exact formula and rounding point for proportional, integral, feedforward, and optional derivative terms.
- Numerical conversion error relative to a rational/high-precision reference must be <= half of one output LSB for a single rounded conversion. Define round-to-nearest with ties away from zero for signed conversions. Integrating profiles must preserve fractional residuals so repeated updates do not accumulate systematic truncation drift.
- Saturate intended control quantities such as duty and integral limits; treat unexpected overflow as a fault. Tests must exercise limits and intermediate overflow, not only final outputs.

The control-algorithm design owns selection and documentation of parameters, coefficients, and numeric representations; user approval of this responsibility does not imply unchosen values have been validated. The initial motion-profile/PI design must publish gain bounds, profile position units, remainder widths, per-step rounding, settling thresholds, and closed-loop acceptance criteria before implementation signoff. These are explicit remaining algorithm contracts, not evidence that revision 0.9 is fully implementation-ready.

#### 24.3 Tracked-vehicle position acceptance

The user approves zero commanded drive after position completion, without active position holding. Completion still requires the configured position error, velocity threshold, and settling interval; zero drive is applied after those criteria have been met. Subsequent drift does not automatically restart position control.

Determine the numerical position tolerance experimentally with the motor coupled to the tracked vehicle. Keep tolerance configurable in integer encoder counts; do not freeze an invented default as an accepted application tolerance. Record the load, surface, commanded distance, direction, speed, and repeated-trial results. Measure encoder error at completion and residual movement after drive removal separately. Vehicle displacement error from track slip or mechanical compliance must be measured independently of motor encoder error.

The system controller uses other localization methods to estimate vehicle position and issue compensating commands. DMOTOR reports motor encoder state and completion; it does not equate encoder position with exact vehicle location or autonomously compensate for track slip. Compensation remains subject to normal queue, watchdog, and HALT authorization.

The measurement campaign must produce the accepted position tolerance, velocity settling threshold, settling duration, and post-completion observation interval. Until those values are established, position-mode tests may use explicitly identified provisional settings, but application-level positioning acceptance remains open. The zero-drive behavior is approved independently of those pending numerical settings.

### 25. Revision history and freeze checklist

| Revision | Scope |
|---|---|
| 0.5 | Original review prompt; supplied text ended at the beginning of implementation phases |
| 0.6 | User architecture decisions, independent timing, MMIO queues, physical-fault inputs, and proposed phases |
| 0.7 | Fault/state contracts, explicit pending decisions, descriptor ownership and reset/drain protocol, completion backpressure, proposed measurable budgets and numeric/estimator acceptance requirements |
| 0.8 | Adopted Pololu DRV8263H #5238; approved 1 RPM minimum and zero drive after position completion; deferred numerical position tolerance to tracked-vehicle measurement |
| 0.9 | Shared escalation for all protective faults; confirmed queue/watchdog and controlled-stop behavior; system-level localization ownership; provisional MMIO through testing; verification criteria and evidence status |
| 0.10 | HLS-first partition; synthesizable command engine based on the common C++ ring; minimal RTL protection/interface boundary retained |
| 0.11 | HLS register decoding and AXI transport; generated-RTL integration evidence; revised proposed MMIO response budget from 8 to 2,048 clocks based on HLS latency and arbitration |
| 0.12 | HLS staged timeout/speed commit and watchdog/recovery register semantics connected to independent RTL safety with live retirement checks |
| 0.13 | C++ HLS motion executor, fractional ramps/PI, position settling, controlled stop, native ring integration reference and generated-RTL scheduler/PWM adapter; complete dispatch/estimator integration remains pending |
| 0.14 | Rational C++ HLS count/period velocity estimator, event snapshot collector, zero/reversal qualification and ideal bidirectional speed sweep; complete controller wiring and physical acceptance remain pending |
| 0.15 | Combined estimator/motion C++ HLS tick with bounded feedback acquisition, capped held duty and sticky fault reporting; full queue/safety/output integration remains pending |
| 0.16 | Shared C++ HLS ring/controller dispatcher with current-boundary acceptance, exact completion retirement and cancellation priority; AXI and physical safety/output adaptation remain pending |
| 0.17 | Single authoritative C++ ring-depth constant, default 4; generated ABI schema 4 and depth-independent wrap/full/backpressure tests |

Before freezing an implementation contract:

- D01, D02, D03a, D03c, D04, and D05 are resolved. Complete D03b for Pololu DRV8263H #5238: disable/coast truth table, current/thermal compatibility, and reset pin biases.
- D07–D10 resolve shared escalation, queue/watchdog behavior, controlled stop, engineering ownership, and test-led MMIO freeze. Verify these rules against the matrix below; revise numerical targets only with documented evidence and affected-test review.
- Supply the machine-readable MMIO map, exact error/interrupt/fault bit encodings, and generated matching RTL/C artifacts.
- Finalize gain formats and bounds, estimator crossover/filter parameters, and profile settling criteria. Validate the approved 1 RPM minimum; establish numerical position tolerance through the D06 tracked-vehicle measurements.
- Verify the platform bus-drain/reset contract, pin synchronization constraints, and independent protection for clock/power loss.
- Demonstrate the initial scheduler, ring, safety, and measurement implementation in both toolchains before expanding features.

The revision 0.9 document update itself did not implement hardware. Subsequent component work and its scoped evidence are tracked in [docs/implementation.md](docs/implementation.md) and [docs/verification.md](docs/verification.md); the full controller is not yet integrated.


#### D03 earlier candidate assessment — superseded by board selection below

The user nominated Digilent PmodHB3 but has not committed to it. The subsequently supplied 2.6 A stall current exceeds its stated 2 A rating by 30%; exclude HB3 from the baseline for unrestricted operation of this motor. Use with external current limiting would require a separate validated current/thermal design. The HTML manual could not be fetched; the manufacturer's rev. E PDF was reviewed instead. Verify the actual board revision before integration.

PmodHB3 provides DIR/EN controls and SA/SB feedback pins, with a stated 2 A bridge and motor voltage up to 12 V. PWM is applied to EN; DIR must not change while EN is high. The documented connector provides neither a dedicated brake control nor a driver-fault/current-sense output. Logic VCC and motor VM are distinct supplies. Its documentation describes coasting between drive pulses; confirm the detailed electrical truth table before using that as the HALT contract. The advertised rating alone does not establish compatibility with the motor's stall current or continuous thermal load. Source: [Digilent PmodHB3 rev. E reference manual](https://digilent.com/reference/_media/pmod:pmod:pmodhb3_rm.pdf).

The proposed HB3 adapter gates EN low on inhibit and preserves DIR until drive is disabled; reversal requires a validated blanking interval. No independent brake capability is claimed. External current/fault monitoring would be required for diagnostic inputs absent from the module. Encoder SA/SB voltage levels must match the FPGA input bank; a feedback connector is not evidence of level conversion. Validate the intended 20 kHz PWM operation rather than assuming it from example waveforms.

A candidate with stronger diagnostics is a TI DRV8876-based board exposing nFAULT, nSLEEP, control inputs, and IPROPI/VREF. The IC supports 4.5–37 V motor power, 3.5 A peak drive, integrated current sensing/regulation, and selectable control modes. Peak current is not a continuous board rating. Analog current telemetry requires an ADC or comparator path. This is an alternative for evaluation, not an approved board selection or purchase recommendation independent of motor current. Source: [TI DRV8876](https://www.ti.com/product/DRV8876).

Resolved inputs: 6 V reference supply; 0.1 A no-load, 0.5 A rated-load, and 2.6 A stall current; initial HALT disables drive and coasts. The exact supplier link remains unavailable.

At the earlier candidate review, D03b remained open for board selection and electrical validation. DRV8876 was not approved: its 3.5 A peak rating alone does not qualify a 2.6 A stalled motor. Select documented transient capability and hardware current regulation appropriate to the motor, and validate continuous thermal performance at the intended load. Set the current limit from startup/load torque and allowable heating requirements, not simply equal to the stall rating.

Verification must cover startup current, bounded injected stall, fault latching, coasting after HALT, and reset/unconfigured-FPGA outputs. Final board selection determines the adapter truth table and pin-to-driver response budget.


#### D03c selected board — 2026-10-11

The user selects the **Pololu DRV8263H Single Brushed DC Motor Driver Carrier #5238**. The designation DRV8226H in the initial selection message was explicitly clarified by the user to mean DRV8263H #5238. This selection supersedes the HB3 and DRV8876 candidates above.

Manufacturer documentation reviewed for this selection specifies 4.5–65 V motor power, 3.3 V-compatible logic, approximately 3.5 A continuous board current subject to operating and thermal conditions, an active-low FAULT output, analog IPROPI current feedback, and a dedicated DRVOFF input that commands high-impedance/coast when high. SLEEP low also commands coast. Source: [Pololu DRV8263H carrier #5238](https://www.pololu.com/product/5238).

The adapter shall use a dedicated hardware inhibit path to DRVOFF; setting PWM to zero alone is not a coast contract. Resolve the control mode and reset biases in the electrical adapter specification, including FPGA-unconfigured, clock-loss, and power-sequencing conditions. Preserve the approved no-active-braking HALT behavior when selecting diagnostic and powered-off-braking settings. Route FAULT into the local protective fault path. IPROPI requires an ADC or comparator interface with validated voltage limits; encoder Hall A/B inputs remain a separate FPGA input path.

Board selection does not approve the default current limit as motor protection. The carrier defaults to approximately 10 A; with its default current-sense scaling, the lowest enabled ITRIP setting is approximately 4 A. A lower hardware limit requires changing the current-sense scaling as well as selecting ITRIP. Select component values and limit tolerances from startup torque, permitted stall duration, motor heating, and loaded measurements. The motor's 2.6 A stall current is not permission for sustained stall. Source: [TI DRV8263-Q1 datasheet, current regulation](https://www.ti.com/lit/gpn/drv8263-q1).

D03b remains open until the adapter configuration, current-limit components, electrical truth table, physical board revision, and thermal/response tests are documented. Adoption of this board is not evidence of completed electrical or safety validation.


### 26. Verification criteria and evidence status

Proceed against Section 24 proposed requirements. Changes based on synthesis or physical measurements must record the previous target, measured evidence, rationale, revised criterion, and affected tests. Preserve approved functional behavior unless explicitly revised. Electrical verification belongs to integration; final positioning tolerance belongs to vehicle acceptance. Neither is claimed by a passing core simulation.

| ID | Test / evidence | Pass criterion |
|---|---|---|
| V01 | Two/four-instance RTL fault injection; each protective class and each source, including simultaneous causes | Source inhibits and asserts HALT_OUT; aggregator latches and all motors inhibit; source attribution retained; no receiver echo; no automatic restart |
| V02 | Watchdog boundary, invalid/stale refresh, full queues, dwell, READY, and controlled stop | Every enabled controller requires its own valid refresh; expiration wins simultaneous refresh; any expiration stops all motors; late refresh cannot resume work |
| V03 | Queue publication, wrap, immutable slots, full completions, and cancellation races | No overwrite/lost accepted work; existing control continues under backpressure only while authorized; independent cancellation reporting survives a full ring |
| V04 | Immediate controlled stop with active and pending commands | Active command superseded, queue canceled, admission closed; watchdog remains active; recovery/session reopening required for new submissions; protective fault wins |
| V05 | Independent RTL timing monitors with bus stalls, stuck HLS, all relevant input/PWM phases | Section 24 local inhibit, HLS deadline, duty application, and MMIO response limits met; deadline-boundary result rejected; shared path measured separately |
| V06 | Encoder and numeric reference tests | Declared capture envelope met; invalid transitions excluded; rational scaling preserved; overflow/rounding rules met; reversals and zero timeout correctly qualified |
| V07 | Ideal estimator and loaded motor model | Both directions at 1/5/30/130 RPM; proposed estimator error <=2% under a documented finite assessment window; algorithm contract supplies settling/window/filter definitions before acceptance; tracking assessed separately |
| V08 | Provisional MMIO schema and bus regression | Generated RTL/software agree on every field and reset value; invalid accesses have specified response/no side effect; snapshots coherent; drain/reset races and version mismatch handled |
| V09 | Both vendor synthesis, static timing, and CDC review | Selected FPGA/profile meets 100 MHz and computation budgets; tool/part/constraints/resource reports retained; CDC assumptions reviewed; host tests alone insufficient |
| V10 | DRV8263H integration and final vehicle trials | Verify current limiting, reset/clock/power-loss behavior, fault/coast response, 1 RPM loaded operation, and measured completion/drift criteria; numerical electrical and vehicle limits documented before acceptance |

Initial evidence review (2026-10-11) preceded implementation and checked requirements consistency and nominal arithmetic only. Subsequent component implementation now has native and RTL tests plus scoped Vitis arithmetic-kernel evidence; see [the verification record](docs/verification.md). V01–V10 are not complete end to end. Integrated timing, CDC, closed-loop behavior, SmartHLS portability, and electrical performance remain unverified.

Nominal arithmetic checks at 100 MHz: one clock is 10 ns; 5 clocks is 50 ns; the 1 kHz tick spans 100,000 clocks; the 80,000-clock HLS deadline is 800 us with 200 us remaining; 20 kHz PWM spans 5,000 clocks (50 us); 2,048-clock proposed MMIO response budget is 20.48 us. The 1,000-clock encoder-state dwell is 10 us, consistent with 100,000 transitions/s. At 10296/5 counts/revolution, 1 RPM gives approximately 29.14 ms between edges and 130 RPM gives 4,461.6 transitions/s. These checks establish arithmetic consistency only, not achieved hardware timing.


