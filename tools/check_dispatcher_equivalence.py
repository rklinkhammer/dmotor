#!/usr/bin/env python3
"""Compare all dispatcher testbench replies against a Git source revision."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--reference', required=True)
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
        sources = sorted((tree / 'src').glob('*.cpp'))
        subprocess.run(['g++', '-std=c++14', '-O2', '-fsanitize=undefined',
                        '-fno-sanitize-recover=all', '-DDMOTOR_TRACE', '-I' + str(tree / 'include'),
                        str(root / 'tests/dispatcher_test.cpp'), *map(str, sources),
                        '-o', str(binary)], check=True)
        traces.append(subprocess.check_output([str(binary)]))
    if traces[0] != traces[1]:
        raise SystemExit('Dispatcher traces differ')
    count = sum(line.endswith(b',') for line in traces[0].splitlines())
    if not count:
        raise SystemExit('No trace records: check DMOTOR_TRACE instrumentation')
    print(f'{count} complete dispatcher replies match {args.reference}; '
          f'SHA256 {hashlib.sha256(traces[0]).hexdigest()}')
