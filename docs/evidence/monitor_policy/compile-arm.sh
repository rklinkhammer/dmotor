#!/bin/bash
set -eu
base=/home/amd-edf/edf/builds/arty-z7-20/tmp/work/cortexa9t2hf-neon-amd-linux-gnueabi/dmotor-semantic-tests/1.0
compiler="$base/recipe-sysroot-native/usr/bin/arm-amd-linux-gnueabi/arm-amd-linux-gnueabi-g++"
export PATH="$base/recipe-sysroot-native/usr/bin/arm-amd-linux-gnueabi:$PATH"
cd /work
mkdir -p build/monitor-policy/arm
for name in sensor_safety_test monitor_policy_test; do
    "$compiler" --sysroot="$base/recipe-sysroot" -mthumb -mfpu=neon -mfloat-abi=hard -mcpu=cortex-a9 -std=c++14 -O2 -Wall -Wextra -Werror -Wconversion -Isoftware/esp32/components/acquisition/include -Isoftware/esp32/components/safety_core/include "software/esp32/tests/$name.cpp" -o "build/monitor-policy/arm/$name"
    "$base/recipe-sysroot-native/usr/bin/qemu-arm" -L "$base/recipe-sysroot" "build/monitor-policy/arm/$name"
done
