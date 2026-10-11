#!/usr/bin/env python3
"""Prepare an isolated, unsynthesized clock experiment from current sources."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--hz', type=int, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
out = args.output.resolve()
if any((root / d) in out.parents for d in ('src', 'include', 'rtl', 'tests', 'tools', 'platforms', 'abi')):
    parser.error('Output must be outside the source directories, e.g. build/clock20')
if out.exists():
    parser.error('Output already exists; use a new directory to preserve evidence')
if args.hz < 1000000 or args.hz > 100000000 or args.hz % 1000000:
    parser.error('Use an integer MHz from 1 through 100')
out.mkdir(parents=True)
for directory in ('src', 'include', 'rtl', 'tests', 'tools', 'platforms', 'abi'):
    shutil.copytree(root / directory, out / directory)
(out / 'docs').mkdir()
shutil.copyfile(root / 'CMakeLists.txt', out / 'CMakeLists.txt')
schema_path = out / 'abi/registers.json'
schema = json.loads(schema_path.read_text())
next(r for r in schema['registers'] if r['name'] == 'CLOCK_HZ')['reset'] = args.hz
schema_path.write_text(json.dumps(schema, indent=2) + '\n')
subprocess.run(['python3', str(out / 'tools/generate_abi.py')], check=True)
print(f'Prepared {args.hz} Hz profile at {out}; synthesis and verification still required')
