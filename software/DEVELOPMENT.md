# PS software development

> Architecture update (2026-11-07): main uses the [1 ms PS motor executor](../DMOTOR_ARCH.md) with independent PL capture/PWM/protection and a matched PSF1 driver/daemon. Client contracts, commissioning-first scope, U0 evidence, the hardware pause and pending live recovery are unchanged.

The [controller programming guide](../docs/controller_programming.md) describes
interfaces and lifecycle rules. This file describes the supported editing and
validation workflow; the root [coding conventions](../docs/coding_standards.md)
also apply. The PS project remains C++14 and builds independently of Vivado/HLS.

## Formatting and review

Use **clang-format 18**, with `software/.clang-format`: LLVM-based, four spaces,
100 columns, attached braces, explicit pointer/reference alignment. The formatter
preserves include order and does not insert braces or rewrite string literals.
New control flow should use braces; automatic layout changes do not add them to
old code. This follows LLVM's [clang-format configuration mechanism](https://clang.llvm.org/docs/ClangFormatStyleOptions.html).

```sh
# From dmotor root:
python3 software/tools/format.py
python3 software/tools/format.py --check
```

The script selects handwritten PS `.cpp`/`.hpp` files in src, include, linux, apps
and tests. Generated `monitor_wire.hpp`, kernel driver C/UAPI, root HLS sources and
future ESP32 firmware are outside its scope. Kernel code follows Linux kernel
conventions and the target kernel's checkpatch/build checks. Change generated
headers through their schema/generator, not the formatter. Use descriptive names,
unit suffixes, RAII resource ownership, bounded work, and checked integer conversion.
Do not claim MISRA, AUTOSAR or functional-safety compliance from formatting/linting.

Keep behavior changes separate in review from mechanical layout changes. Inspect
`git diff --check`; include adverse-path tests for ownership, stale sessions,
malformed input and transport failure when those behaviors change.

## Native build, analysis and tests

On Ubuntu 24.04 the developer packages are `build-essential cmake ninja-build
python3 clang-format-18 clang-tidy-18`. Run from the repository root:

```sh
cmake -S software -B build/ps-dev \
  -DCMAKE_BUILD_TYPE=Debug -DDMOTOR_SW_SANITIZE=ON -DDMOTOR_SW_CLANG_TIDY=ON
cmake --build build/ps-dev -j4
cmake --build build/ps-dev --target ps-format-check
ctest --test-dir build/ps-dev --output-on-failure
```

The existing `-Wall -Wextra -Werror -Wconversion` baseline remains in place.
`DMOTOR_SW_SANITIZE` enables UBSan; `DMOTOR_SW_CLANG_TIDY` runs clang-tidy 18 during
compilation with the focused analyzer/bug checks in `.clang-tidy`, treating enabled
warnings as errors. Fix or document a specific false positive instead of disabling
an entire check family. No automatic clang-tidy fixes run. Analysis uses real build
flags, including separately compiled process fixtures. If changing the lint config,
use a clean build (`--clean-first`) so unchanged translation units are reanalyzed.

CMake always exports `build/ps-dev/compile_commands.json`. Point clangd at that
build directory (`--compile-commands-dir=.../build/ps-dev`) or set the C/C++ editor's
compileCommands setting to the absolute JSON path. Configure/build once to create
the generated version header. Use the same clang-format 18 binary for editor
format-on-save. Do not check generated build directories or compilation databases
into source control. See LLVM's [clang-tidy guide](https://clang.llvm.org/extra/clang-tidy/)
for the compilation database workflow.

Developer tools are opt-in, not required for ordinary CMake/Yocto builds. Both
options default OFF. Cross builds must use the EDF toolchain/sysroot, not native
analysis commands. This workflow tests software semantics and process behavior;
it does not qualify scheduling, kernel MMIO, electrical inhibition or monitor ARM.

## CI and packaging

`.github/workflows/ps-software.yml` repeats formatting, native analysis/UBSan,
monitor schema validation (through CTest), and tests on pull requests/pushes that
change PS software or its shared controller headers/helpers. It does not flash,
load a driver, publish an image, or build the FPGA.

`software/sources.json` lists staged/provenance inputs. Add new required build
scripts/configs there so EDF's isolated source archive remains buildable. For a
new release, use the current EDF staging/build workflow in [build.md](../docs/build.md)
and EDF's build guide; never edit installed source-version metadata to claim a
new source tree matches an old binary. Formatting changes also change source hashes.

## Daemon application development

Use the [client library](CLIENT.md) for all daemon calls. Link `dmotor::client`;
do not include the internal wire header or duplicate socket/MMIO access in an app.
CTest includes client transport failures, daemon integration through maintenance,
and a separate installed CMake consumer. The client is packaged independently of
the daemon's runtime/MMIO implementation.
