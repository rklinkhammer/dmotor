#!/usr/bin/env python3
"""Prepare the isolated 200 RPM fixed-point velocity evaluation at 20 MHz."""
import argparse
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--variant', choices=['q7', 'exact', 'tenths'], default='exact')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
output = args.output.resolve()
subprocess.run([sys.executable, str(root / 'tools/prepare_velocity_profile.py'),
                '--output', str(output)], check=True)
patch_path = (root / 'docs/evidence/velocity_tenths/profile.patch' if args.variant == 'tenths'
              else root / 'docs/evidence/velocity_bounded' /
              ('profile.patch' if args.variant == 'q7' else 'exact.patch'))
subprocess.run(['patch', '--batch', '-p1', '--directory', str(output), '--input',
                str(patch_path)], check=True)
print(f'Prepared bounded velocity evaluation at {output}; validation still required')
