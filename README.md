# DMOTOR

Portable single-motor FPGA controller under development, following
[DMOTOR_ARCH.md](DMOTOR_ARCH.md). The first implementation provides independently
tested C++ HLS command-engine, register peripheral, motion-executor and velocity
estimator components, plus RTL protection, scheduling, encoder and PWM components. The architecture now prefers C++ HLS
wherever practical. It is **not
an integrated motor controller or a board-ready bitstream**.

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

Vitis checks the selected kernel for `xc7z020clg400-1` at 100 MHz using C
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
