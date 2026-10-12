#!/usr/bin/env python3
"""Compare exact bounded velocity behavior with the full source implementation."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--profile', type=Path, required=True)
parser.add_argument('--reference-tests', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
profile = args.profile.resolve()
with tempfile.TemporaryDirectory(prefix='dmotor-velocity-profile-') as directory:
    for name in ('motion', 'controller', 'dispatcher'):
        traces = []
        for label, tree in [('baseline', root), ('velocity_only', profile)]:
            binary = Path(directory) / f'{name}_{label}'
            subprocess.run(['g++', '-std=c++14', '-O2', '-fsanitize=undefined',
                            '-fno-sanitize-recover=all', '-DDMOTOR_TRACE',
                            '-I' + str(tree / 'include'), '-I' + str(profile / 'tests'),
                            str(args.reference_tests / f'{name}_test.cpp'),
                            *map(str, sorted((tree / 'src').glob('*.cpp'))),
                            '-o', str(binary)], check=True)
            traces.append(subprocess.check_output([str(binary)]))
        if traces[0] != traces[1]:
            raise SystemExit(f'{name}: retained-profile outputs differ')
        count = sum(line.endswith(b',') for line in traces[0].splitlines())
        if not count:
            raise SystemExit(f'{name}: no trace records')
        print(f'{name}: {count} retained-profile replies match; '
              f'SHA256 {hashlib.sha256(traces[0]).hexdigest()}', flush=True)
