#!/usr/bin/env python3
"""Prepare an isolated two-motor shared velocity engine for measurement."""
import argparse
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--variant', choices=['direct', 'copy'], default='direct')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
output = args.output.resolve()
subprocess.run([sys.executable, str(root / 'tools/prepare_velocity_profile.py'),
                '--output', str(output)], check=True)
subprocess.run(['patch', '--batch', '-p1', '--directory', str(output), '--input',
                str(root / 'docs/evidence/velocity_pair' / f'{args.variant}.patch')], check=True)
print(f'Prepared {args.variant} two-motor profile at {output}; validation still required')
