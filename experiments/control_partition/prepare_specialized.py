#!/usr/bin/env python3
"""Experimental mailbox-only specialization; not a replacement for generic APIs.
The prepared product is already velocity-only (opcodes 1 and 6). This candidate
further specializes to VelocityMailbox's sole descriptor opcode 1; controlled
STOP continues through immediate_stop/stopping_. Do not use with generic motion
or dispatcher callers. Original source and default build remain unchanged.
"""
import argparse
from pathlib import Path
import subprocess
import sys
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
subprocess.run([sys.executable, str(root/'tools/prepare_shared_mailbox.py'),
                '--output', str(a.output)], check=True)
out = a.output.resolve()
p = out/'src/motion.cpp'
s = p.read_text()
assert 'target_position_' not in s and 'duration_us_' not in s
assert s.count('opcode_ = static_cast<uint8_t>(command[0] & 7U);') == 1
s = s.replace('opcode_ = static_cast<uint8_t>(command[0] & 7U);',
              '// Mailbox-only profile: descriptor opcode is always 1.')
s = s.replace('opcode_ == 1', 'true').replace('opcode_ == 6', 'false')
s = s.replace('    opcode_ = 0;\n', '')
assert 'opcode_' not in s
p.write_text(s)
p = out/'include/dmotor/motion.hpp'
s = p.read_text()
s = '\n'.join(x for x in s.splitlines() if 'uint8_t opcode_' not in x)+'\n'
p.write_text(s)
print('EXPERIMENT ONLY: generic opcode-6 API is not supported; shared mailbox STOP is retained.')
