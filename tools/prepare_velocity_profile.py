#!/usr/bin/env python3
"""Create an isolated velocity-only evaluation; leave the active profile intact."""
import argparse
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
output = args.output.resolve()
subprocess.run([sys.executable, str(root / 'tools/prepare_clock_profile.py'),
                '--hz', '20000000', '--output', str(output)], check=True)
subprocess.run(['patch', '--batch', '-p1', '--directory', str(output), '--input',
                str(root / 'docs/evidence/velocity_only/profile.patch')], check=True)
print(f'Prepared velocity-only 20 MHz evaluation at {output}; validation still required')
