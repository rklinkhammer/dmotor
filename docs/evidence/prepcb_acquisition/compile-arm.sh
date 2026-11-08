#!/bin/bash
set -eu
base=/home/amd-edf/edf/builds/arty-z7-20/tmp/work/cortexa9t2hf-neon-amd-linux-gnueabi/dmotor-semantic-tests/1.0
compiler="$base/recipe-sysroot-native/usr/bin/arm-amd-linux-gnueabi/arm-amd-linux-gnueabi-g++"
export PATH="$base/recipe-sysroot-native/usr/bin/arm-amd-linux-gnueabi:$PATH"
cd /work
mkdir -p build/prepcb/arm
"$compiler" --sysroot="$base/recipe-sysroot" -mthumb -mfpu=neon -mfloat-abi=hard -mcpu=cortex-a9 -std=c++14 -O2 -Wall -Wextra -Werror -Wconversion -Isoftware/include software/apps/acquisition_test.cpp -o build/prepcb/arm/dmotor-acquisition-test
"$base/recipe-sysroot-native/usr/bin/qemu-arm" -L "$base/recipe-sysroot" build/prepcb/arm/dmotor-acquisition-test --help
"$compiler" --sysroot="$base/recipe-sysroot" -mthumb -mfpu=neon -mfloat-abi=hard -mcpu=cortex-a9 -std=c++14 -O2 -Wall -Wextra -Werror -Wconversion -Isoftware/include software/esp32/tests/acquisition_link_test.cpp -o build/prepcb/arm/acquisition_link_test
"$base/recipe-sysroot-native/usr/bin/qemu-arm" -L "$base/recipe-sysroot" build/prepcb/arm/acquisition_link_test
