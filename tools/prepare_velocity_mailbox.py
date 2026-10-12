#!/usr/bin/env python3
"""Prepare an isolated velocity mailbox experiment using the exact one-phase core."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
out = a.output.resolve()
subprocess.run([sys.executable, str(root / 'tools/prepare_bounded_velocity.py'),
                '--variant', 'exact-onephase', '--output', str(out)], check=True)
for name, destination in [('mailbox.hpp', 'include/dmotor/mailbox.hpp'),
                           ('mailbox.cpp', 'src/mailbox.cpp'),
                           ('mailbox_test.cpp', 'tests/mailbox_test.cpp')]:
    shutil.copyfile(root / 'experiments/mailbox' / name, out / destination)
cmake = out / 'CMakeLists.txt'
cmake.write_text(cmake.read_text() + '\ntarget_sources(dmotor PRIVATE src/mailbox.cpp)\n'
                 'add_executable(mailbox_test tests/mailbox_test.cpp)\n'
                 'target_link_libraries(mailbox_test PRIVATE dmotor)\n'
                 'add_test(NAME mailbox COMMAND mailbox_test)\n')
config = (out / 'platforms/vitis/controller.cfg').read_text()
config = config.replace('syn.top=dmotor_controller',
                        'syn.top=dmotor_mailbox\nsyn.file=../../src/mailbox.cpp')
config = config.replace('tb.file=../../tests/controller_test.cpp',
                        'tb.file=../../tests/mailbox_test.cpp')
(out / 'platforms/vitis/dispatcher.cfg').write_text(config)
print(f'Prepared mailbox measurement at {out}; dispatcher runner synthesizes dmotor_mailbox')
