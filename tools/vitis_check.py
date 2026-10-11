#!/usr/bin/env python3
"""C simulation, synthesis, and RTL co-simulation for the selected HLS kernel."""
import argparse
from pathlib import Path
import subprocess
p = argparse.ArgumentParser()
p.add_argument('--vitis', type=Path, default=Path('/opt/vitis/2026.1/Vitis'))
p.add_argument('--kernel', choices=['pi', 'command', 'peripheral', 'motion', 'velocity', 'controller', 'dispatcher'], default='pi')
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
(root / 'build').mkdir(exist_ok=True)
config = 'hls.cfg' if a.kernel == 'pi' else a.kernel + '.cfg'
work = 'vitis_component' if a.kernel == 'pi' else 'vitis_' + a.kernel
common = ['--mode', 'hls', '--config', config, '--work_dir', '../../build/' + work]
steps = [('csim', 'vitis-run', ['--csim']), ('synth', 'v++', ['--compile']),
         ('cosim', 'vitis-run', ['--cosim'])]
for label, binary, flags in steps:
    log = root / 'build' / (f'vitis_{label}.log' if a.kernel == 'pi' else f'vitis_{a.kernel}_{label}.log')
    print(f'{label}: {log}', flush=True)
    with log.open('w') as output:
        result = subprocess.run([str(a.vitis / 'bin' / binary), *flags, *common],
                                cwd=root / 'platforms/vitis', stdout=output,
                                stderr=subprocess.STDOUT)
    if result.returncode:
        print(log.read_text()[-6000:])
        raise SystemExit(result.returncode)
