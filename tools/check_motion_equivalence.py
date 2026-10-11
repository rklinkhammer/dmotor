#!/usr/bin/env python3
"""Compare motion outputs against a Git revision with its own headers and sources."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--reference', required=True, help='Git revision containing baseline motion.cpp')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='dmotor-equivalence-') as directory:
    work = Path(directory)
    baseline = work / 'baseline'
    baseline.mkdir()
    archive = subprocess.check_output(
        ['git', 'archive', args.reference, 'include', 'src'], cwd=root)
    subprocess.run(['tar', '-x', '-C', str(baseline)], input=archive, check=True)
    traces = []
    for label, tree in [('reference', baseline), ('current', root)]:
        binary = work / label
        sources = [tree / 'src/motion.cpp', tree / 'src/protocol.cpp']
        subprocess.run(['g++', '-std=c++14', '-O2', '-fsanitize=undefined',
                        '-fno-sanitize-recover=all', '-I' + str(tree / 'include'),
                        str(root / 'tests/motion_trace.cpp'), *map(str, sources),
                        '-o', str(binary)], check=True)
        traces.append(subprocess.check_output([str(binary)]))
    if traces[0] != traces[1]:
        raise SystemExit('Motion traces differ')
    print(f'{len(traces[0].splitlines())} motion snapshots match {args.reference}; '
          f'SHA256 {hashlib.sha256(traces[0]).hexdigest()}')
