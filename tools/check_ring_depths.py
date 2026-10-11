#!/usr/bin/env python3
"""Check native tests and generated ABI at alternate depths in isolated copies."""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('depths', nargs='*', type=int, default=[2, 4, 16])
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
for depth in args.depths:
    if not (2 <= depth <= 64 and depth & (depth - 1) == 0):
        parser.error('depth must be a power of two from 2 to 64')
    with tempfile.TemporaryDirectory(prefix=f'dmotor-depth-{depth}-') as directory:
        target = Path(directory)
        for name in ('src', 'include', 'tests', 'tools', 'abi'):
            shutil.copytree(root / name, target / name)
        (target / 'docs').mkdir()
        (target / 'rtl').mkdir()
        shutil.copy2(root / 'CMakeLists.txt', target / 'CMakeLists.txt')
        header = target / 'include/dmotor/ring_config.hpp'
        header.write_text(re.sub(r'ring_depth = [0-9]+;', f'ring_depth = {depth};', header.read_text()))
        commands = [
            ['python3', 'tools/generate_abi.py'],
            ['cmake', '-S', '.', '-B', 'build', '-DIVERILOG=IVERILOG-NOTFOUND', '-DVVP=VVP-NOTFOUND'],
            ['cmake', '--build', 'build', '-j2'],
            ['ctest', '--test-dir', 'build', '--output-on-failure'],
        ]
        for command in commands:
            result = subprocess.run(command, cwd=target, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if result.returncode:
                print(result.stdout)
                raise SystemExit(result.returncode)
        print(f'depth {depth}: native tests and generated ABI passed', flush=True)
