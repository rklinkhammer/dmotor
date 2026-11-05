#!/usr/bin/env python3
"""Format/check handwritten PS userspace C++ with clang-format 18."""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--check', action='store_true')
parser.add_argument('--tool', default='clang-format-18')
args = parser.parse_args()
tool = shutil.which(args.tool)
if not tool:
    parser.error(f'{args.tool} not found; install clang-format 18')
version = subprocess.check_output([tool, '--version'], text=True)
if not re.search(r'version 18\.', version):
    parser.error(f'clang-format 18 required; got {version.strip()}')
files = sorted(p for folder in ('src', 'include', 'linux', 'apps', 'tests')
               for p in (root / folder).rglob('*')
               if p.suffix in ('.cpp', '.hpp') and p.name != 'monitor_wire.hpp')
# Kernel C/UAPI and generated headers keep their own upstream/generator style.
command = [tool, '--style=file']
command += ['--dry-run', '--Werror'] if args.check else ['-i']
result = subprocess.run(command + [str(p) for p in files])
if result.returncode == 0:
    print(f'{"Checked" if args.check else "Formatted"} {len(files)} PS C++ files')
sys.exit(result.returncode)
