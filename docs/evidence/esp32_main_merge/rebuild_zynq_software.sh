#!/bin/bash
set -euo pipefail
cd /home/rklinkhammer/workspace/edf-dev
export EDF_DMOTOR_SOURCE=/home/rklinkhammer/workspace/dmotor
./edf --target arty-z7-20 --machine board bitbake -c cleansstate xilinx-bootbin u-boot-xlnx device-tree mc:arty-z7-20-cortexa9-fsbl:fsbl-firmware
./edf --target arty-z7-20 --machine linux bitbake -c cleansstate linux-xlnx dmotor-semantic-tests dmotor-access dvrmc-firmware edf-linux-disk-image
./edf --target arty-z7-20 image
./edf --target arty-z7-20 export
