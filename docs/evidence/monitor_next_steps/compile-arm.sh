#!/bin/bash
set -eu
base=/home/amd-edf/edf/builds/arty-z7-20/tmp/work/cortexa9t2hf-neon-amd-linux-gnueabi/dmotor-semantic-tests/1.0
compiler="$base/recipe-sysroot-native/usr/bin/arm-amd-linux-gnueabi/arm-amd-linux-gnueabi-g++"
export PATH="$base/recipe-sysroot-native/usr/bin/arm-amd-linux-gnueabi:$PATH"
cd /work
mkdir -p build/monitor-next/arm
"$compiler" --sysroot="$base/recipe-sysroot" -mthumb -mfpu=neon -mfloat-abi=hard -mcpu=cortex-a9 -std=c++14 -O2 -Wall -Wextra -Werror -Wconversion -Isoftware/include software/apps/acquisition_test.cpp -o build/monitor-next/arm/dmotor-acquisition-test
"$base/recipe-sysroot-native/usr/bin/qemu-arm" -L "$base/recipe-sysroot" build/monitor-next/arm/dmotor-acquisition-test --help
"$compiler" --sysroot="$base/recipe-sysroot" -mthumb -mfpu=neon -mfloat-abi=hard -mcpu=cortex-a9 -std=c++14 -O2 -Wall -Wextra -Werror -Wconversion -Isoftware/include software/esp32/tests/acquisition_link_test.cpp -o build/monitor-next/arm/acquisition_link_test
"$base/recipe-sysroot-native/usr/bin/qemu-arm" -L "$base/recipe-sysroot" build/monitor-next/arm/acquisition_link_test

"$compiler" --sysroot="$base/recipe-sysroot" -mthumb -mfpu=neon -mfloat-abi=hard -mcpu=cortex-a9 -std=c++14 -O2 -Wall -Wextra -Werror -Wconversion -Isoftware/esp32/components/acquisition/include -Isoftware/esp32/components/safety_core/include software/esp32/tests/sensor_safety_test.cpp -o build/monitor-next/arm/sensor_safety_test
"$base/recipe-sysroot-native/usr/bin/qemu-arm" -L "$base/recipe-sysroot" build/monitor-next/arm/sensor_safety_test
