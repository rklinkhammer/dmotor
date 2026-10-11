#!/usr/bin/env python3
"""Compare motion outputs against a Git revision using the same protocol code."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--reference', required=True, help='Git revision containing baseline motion.cpp')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
reference = subprocess.check_output(['git', 'show', f'{args.reference}:src/motion.cpp'], cwd=root)
with tempfile.TemporaryDirectory(prefix='dmotor-motion-equivalence-') as directory:
    work = Path(directory)
    source = work / 'reference.cpp'
    source.write_bytes(reference)
    traces = []
    for label, implementation in [('reference', source), ('current', root / 'src/motion.cpp')]:
        binary = work / label
        subprocess.run(['g++', '-std=c++14', '-O2', '-fsanitize=undefined',
                        '-fno-sanitize-recover=all', '-I' + str(root / 'include'),
                        str(root / 'tests/motion_trace.cpp'), str(implementation),
                        str(root / 'src/protocol.cpp'), '-o', str(binary)], check=True)
        traces.append(subprocess.check_output([str(binary)]))
    if traces[0] != traces[1]:
        raise SystemExit('Motion traces differ')
    print(f'{len(traces[0].splitlines())} motion updates match {args.reference}; '
          f'SHA256 {hashlib.sha256(traces[0]).hexdigest()}')
