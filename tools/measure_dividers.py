#!/usr/bin/env python3
"""Synthesize and co-simulate an isolated width sweep; no production changes."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
import json
import xml.etree.ElementTree as ET

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--jobs', type=int, default=3)
p.add_argument('--vitis', type=Path, default=Path('/opt/vitis/2026.1/Vitis'))
p.add_argument('--tailored', action='store_true', help='Sweep asymmetric PI numerator/denominator widths')
p.add_argument('--sizes', help='Comma-separated subset, e.g. 32,39,64 or 32x14')
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=False)

def run(width):
    original = width == 'original64'
    bits = 64 if original else int(str(width).split('x')[0])
    den_bits = int(str(width).split('x')[1]) if 'x' in str(width) else bits
    folder = out / str(width)
    folder.mkdir()
    source = folder / 'top.cpp'
    source.write_text('#include "divider.hpp"\nap_uint<%d> divider_top(ap_uint<%d> n, ap_uint<%d> d) {\n return divide_sized<%d,%d>(n,d);\n}\n' % (bits, bits, den_bits, bits, den_bits))
    top = 'dmotor::detail::divide' if original else 'divider_top'
    files = [root / 'src/arithmetic.cpp'] if original else [source]
    flags = f'-I{root / "experiments/divider"} -I{root / "include"} -DDIVIDER_WIDTH={bits} -DDENOMINATOR_WIDTH={den_bits}'
    if original:
        flags += ' -DORIGINAL_DIVIDER'
    config = folder / 'hls.cfg'
    config.write_text(f'part=xc7z020clg400-1\nfreqhz=20000000\n[hls]\nsyn.top={top}\n' +
                      ''.join(f'syn.file={f}\n' for f in files) +
                      f'syn.cflags={flags}\nsyn.rtl.reset=all\ntb.file={root / "experiments/divider/bench.cpp"}\ntb.cflags={flags}\n')
    with (folder / 'native.log').open('w') as log:
        subprocess.run(['g++', '-std=c++14', '-O2', '-DEXTENDED_TEST',
                        f'-I{a.vitis / "include"}', *flags.split(),
                        str(root / 'experiments/divider/bench.cpp'), *map(str, files),
                        '-o', str(folder / 'native')], stdout=log, stderr=subprocess.STDOUT, check=True)
        subprocess.run([str(folder / 'native')], stdout=log, stderr=subprocess.STDOUT, check=True)
    for stage, exe, option in [('csim','vitis-run','--csim'),('synth','v++','--compile'),('cosim','vitis-run','--cosim')]:
        print(f'{width}: {stage}', flush=True)
        with (folder / f'{stage}.log').open('w') as log:
            subprocess.run([str(a.vitis / 'bin' / exe), option, '--mode', 'hls',
                            '--config', str(config), '--work_dir', str(folder / 'work')],
                           cwd=folder, stdout=log, stderr=subprocess.STDOUT, check=True)
    reports = folder / 'work/hls/syn/report'
    xml = ET.parse(reports / ('dmotor_detail_divide_csynth.xml' if original else 'divider_top_csynth.xml'))
    result = {'width': str(width)}
    for key in ['LUT','FF','DSP','BRAM_18K']:
        result[key] = int(xml.findtext(f'.//AreaEstimates/Resources/{key}'))
    for key in ['Best-caseLatency','Worst-caseLatency','Interval-min','Interval-max']:
        result[key] = xml.findtext(f'.//SummaryOfOverallLatency/{key}')
    result['EstimatedClockPeriod'] = xml.findtext('.//EstimatedClockPeriod')
    (folder / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(result, flush=True)
    return result

with ThreadPoolExecutor(max_workers=a.jobs) as pool:
    sizes = ['32x4','30x14','32x14','36x8','33x17','36x17','39x10','36x20','39x20'] if a.tailored else [16,24,30,32,33,36,39,48,64,'original64']
    if a.sizes:
        sizes = a.sizes.split(',')
    results = list(pool.map(run, sizes))
(out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
