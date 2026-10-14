#!/usr/bin/env python3
"""Stage this checkout's semantic-test recipe in an EDF layer; no build mutation."""
import argparse
from pathlib import Path
import shutil
import tarfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("edf", type=Path, help="edf-dev checkout")
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
layer = args.edf.resolve() / "meta-edf-dev"
if not (layer / "conf/layer.conf").is_file():
    parser.error("Not an EDF checkout with meta-edf-dev")
destination = layer / "recipes-dmotor/dmotor-semantic-tests"
(destination / "files").mkdir(parents=True, exist_ok=True)
# Explicit source list: never archive build output, credentials or unrelated files.
files = ["software/CMakeLists.txt", "software/include/dmotor_sw/model.hpp",
         "software/src/model.cpp", "software/tests/model_test.cpp",
         "src/atomic_mailbox.cpp", "include/dmotor/atomic_mailbox.hpp",
         "include/dmotor/batch_host.hpp", "include/dmotor/batch_registers.hpp",
         "software/include/dmotor_sw/device.hpp", "software/include/dmotor_sw/protocol.hpp",
         "software/src/protocol.cpp", "software/tests/protocol_test.cpp",
         "include/dmotor/controller_host.hpp", "include/dmotor/controller_registers.hpp",
         "include/dmotor/timing.hpp", "include/dmotor/registers.hpp", "include/dmotor/ring_config.hpp",
         "software/include/dmotor_sw/linux_inspection.hpp", "software/linux/inspection.cpp",
         "software/apps/inspect.cpp", "software/tests/linux_inspection_test.cpp"]
with tarfile.open(destination / "files/dmotor-software.tar.gz", "w:gz") as archive:
    for name in files:
        archive.add(root / name, arcname="dmotor-software/" + name)
shutil.copyfile(root / "software/yocto/dmotor-semantic-tests.bb",
                destination / "dmotor-semantic-tests.bb")
print(destination)
