#!/usr/bin/env python3
"""Prepare the isolated four-motor shared mailbox measurement."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--variant', choices=['direct', 'copy'], default='copy')
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
out = a.output.resolve()
subprocess.run([sys.executable, str(root / 'tools/prepare_velocity_mailbox.py'),
                '--output', str(out)], check=True)
for name, dest in [('shared_mailbox.hpp', 'include/dmotor/shared_mailbox.hpp'),
                   ('shared_mailbox.cpp', 'src/shared_mailbox.cpp'),
                   ('shared_mailbox_test.cpp', 'tests/shared_mailbox_test.cpp')]:
    shutil.copyfile(root / 'experiments/mailbox' / name, out / dest)
cmake = out / 'CMakeLists.txt'
with cmake.open('a') as f:
    f.write('\ntarget_sources(dmotor PRIVATE src/shared_mailbox.cpp)\n'
            'add_executable(shared_mailbox_test tests/shared_mailbox_test.cpp)\n'
            'target_link_libraries(shared_mailbox_test PRIVATE dmotor)\n'
            'add_test(NAME shared_mailbox COMMAND shared_mailbox_test)\n')
cfg = out / 'platforms/vitis/dispatcher.cfg'
s = cfg.read_text().replace('syn.top=dmotor_mailbox',
    'syn.top=dmotor_mailbox_shared\nsyn.file=../../src/shared_mailbox.cpp')
s = s.replace('tb.file=../../tests/mailbox_test.cpp',
              'tb.file=../../tests/shared_mailbox_test.cpp')
if a.variant == 'copy':
    s = s.replace('-DDMOTOR_USE_AP_INT', '-DDMOTOR_USE_AP_INT -DDMOTOR_MAILBOX_COPY_CONTEXT')
    with cmake.open('a') as f:
        f.write('target_compile_definitions(dmotor PRIVATE DMOTOR_MAILBOX_COPY_CONTEXT)\n')
cfg.write_text(s)
print(f'Prepared four-motor mailbox at {out}; dispatcher runner uses dmotor_mailbox_shared')
