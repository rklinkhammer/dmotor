# DMOTOR

Portable single-motor FPGA controller under development, following
[DMOTOR_ARCH.md](DMOTOR_ARCH.md). The first implementation provides independently
tested C++ HLS command-engine, register peripheral, motion-executor and velocity
estimator components, plus RTL protection, scheduling, encoder and PWM components. The architecture now prefers C++ HLS
wherever practical. It is **not
an integrated motor controller or a board-ready bitstream**.

The system target is at most four motors with independent controllers. Current
resource estimates do not yet fit four instances on xc7z020clg400-1; see
[verification results](docs/verification.md) for measured optimization progress.

The active full profile uses exact 64-bit division with one restoring phase per
bit; see the [measured before/after results](docs/evidence/onephase_main/README.md).

## Build and test

Requires CMake, a C++14 compiler, Python 3, and Icarus Verilog (`iverilog`/`vvp`)
for RTL tests. Native arithmetic runs with undefined-behavior sanitization by default.

```sh
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

CMake warns if RTL tools are unavailable; a native-only pass is not an RTL pass.
Custom installations can set `-DIVERILOG=/path/iverilog -DVVP=/path/vvp` and, for
relocated Icarus packages, `-DIVERILOG_BASE=/path/to/ivl`.

```sh
python3 tools/generate_abi.py          # regenerate draft interface artifacts
python3 tools/generate_abi.py --check  # verify generated files are current
python3 tools/vitis_check.py          # PI; defaults to /opt/vitis/2026.1/Vitis
python3 tools/vitis_check.py --kernel command # stateful command engine
python3 tools/vitis_check.py --kernel peripheral # HLS register peripheral
python3 tools/vitis_check.py --kernel motion # profiles, PI and command execution
python3 tools/vitis_check.py --kernel velocity # rational encoder speed estimation
python3 tools/vitis_check.py --kernel controller # combined estimator/motion tick
python3 tools/vitis_check.py --kernel dispatcher # shared queue/controller owner
```

Vitis checks the selected kernel for `xc7z020clg400-1` at 20 MHz using C
simulation, synthesis, and RTL co-simulation. Outputs live in `build/`.
Override the installation with `--vitis /path/to/Vitis`. No physical programming,
IP deployment, or board power-up is performed by these commands.

## Command ring capacity

Submission and completion rings share one compile-time constant,
`dmotor::ring_depth`, in [ring_config.hpp](include/dmotor/ring_config.hpp).
The default is **4 entries**; supported depths are powers of two from 2 through 64.
Storage, wrap masks, and generated C++/RTL slot counts derive from this value.
Descriptor size remains 16 words (64 bytes), independently of ring depth.

After changing the constant, regenerate the interface and rebuild the host code
and affected HLS kernels together:

```sh
python3 tools/generate_abi.py
python3 tools/check_ring_depths.py  # native regression at depths 2, 4, and 16
```

The depth-4 default uses provisional ABI schema version 4. Register offsets and
slot strides are unchanged; software must use the generated capacity constants.

## Timeout and encoder timing limits

[include/dmotor/timing.hpp](include/dmotor/timing.hpp) defines the named cycle
limits used by the velocity estimator and controller `step()` functions. Values
are derived from the fabric clock: minimum zero/acquisition timeout is 1 ms,
maximum zero timeout and encoder interval are 10 seconds, and minimum encoder
edge spacing is 10 microseconds. Actual zero/acquisition timeouts remain runtime
configuration values; acquisition must be shorter than the zero timeout.
Changing these limits requires rebuilding the affected HLS kernels and reviewing
the timing/numeric contract. Time-based limits are converted using the active fabric clock.

## Review material

- [Target Vivado project reference](docs/vivado_reference.md)
- [HLS queue/controller dispatcher](docs/dispatcher.md)
- [Combined HLS controller and acquisition policy](docs/controller.md)
- [HLS velocity estimator](docs/velocity_estimator.md)
- [HLS motion executor and scheduler/PWM adapter](docs/motion_executor.md)
- [Safety/configuration registers](docs/safety_configuration.md)
- [AXI/HLS peripheral and timing contract](docs/axi_peripheral.md)
- [HLS command-engine interface](docs/command_engine.md)
- [Implementation boundaries and numerical contract](docs/implementation.md)
- [Verification results and remaining gates](docs/verification.md)
- [Generated provisional register table](docs/registers.md)
- [Machine-readable register source](abi/registers.json)
- [Coding conventions](docs/coding_standards.md)

Board selection is Pololu DRV8263H #5238. Electrical current limiting, reset
biases, power/clock-loss behavior, and vehicle position tolerance remain integration
work. SmartHLS validation and the full motion-controller integration remain pending. AXI-to-HLS ring integration
is now tested against generated RTL; safety/watchdog configuration is now connected; the motion executor is tested separately with its scheduler/PWM adapter.
The estimator and motion executor now have a combined C++ HLS coordinator with
bounded feedback acquisition and a shared HLS queue/controller dispatcher.
AXI adaptation, complete RTL/safety wiring and motor tuning configuration remain work.

### Timing profile

Timing budgets are named in `abi/timing.json`; fabric frequency is CLOCK_HZ in
`abi/registers.json`. Run `python3 tools/generate_abi.py` after edits to regenerate
C++/RTL timing constants and synchronize Vitis clock constraints. The approved
external HALT digital-inhibit budget is 10 us, with immediate hardware reaction;
it excludes motor-driver and mechanical stopping delays. The active clock is
20 MHz: 20,000 clocks per control tick, 1,000 clocks per PWM frame, and a
16,000-clock computation deadline. PWM resolution is 0.1% per clock. The local
five-clock HALT pipeline bound is 250 ns, within the 10 us budget.
See `docs/verification.md` for profile-change validation. The reference Vivado
design requires clock/reset integration changes; see `docs/vivado_reference.md`.
RTL projects must compile `rtl/timing.sv` before `scheduler.sv` and `pwm.sv`.
