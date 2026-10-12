#!/usr/bin/env python3
"""Compare transport + generated four-context motor RTL with the C++ reference."""
import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--iverilog', default='iverilog')
p.add_argument('--vvp', default='vvp')
p.add_argument('--base', default='')
p.add_argument('--experiment', type=Path, default=Path('build/shared_mailbox_copy'))
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
experiment = (a.experiment if a.experiment.is_absolute() else root / a.experiment).resolve()
generated = experiment / 'build/vitis_dispatcher/hls/syn/verilog'
if not (generated / 'dmotor_mailbox_shared.v').exists():
    raise SystemExit('Prepare/synthesize the shared mailbox copy experiment first; see docs/shared_mailbox.md')
out = root / 'build/shared_engine_transport'
out.mkdir(parents=True, exist_ok=True)
# Instrument the existing independent-context test to export its checked reference
# transactions. Keep the prepared experiment and production C++ sources unchanged.
source = (experiment / 'tests/shared_mailbox_test.cpp').read_text()
source = source.replace('#include <iostream>', '#include <iostream>\n#include <fstream>\n#include <iomanip>\nstatic std::ofstream vectors("vectors.txt");')
marker = '    ++compared;'
if source.count(marker) != 1:
    raise SystemExit('Shared reference test changed: review vector export hook')
source = source.replace(marker, '''    const uint32_t flags = unsigned(reset) | (unsigned(tick)<<1) |
        (unsigned(c.authorized)<<2) | (unsigned(stop)<<3) | (unsigned(fault)<<4) |
        (unsigned(c.negative)<<5);
    const uint32_t scalars[] = {motor, flags, c.session, uint32_t(c.now),uint32_t(c.now>>32),
        uint32_t(c.count),uint32_t(c.count>>32),uint32_t(c.edge),uint32_t(c.edge>>32),c.direction,0};
    auto word = [](uint32_t v) { vectors << std::hex << std::setw(8) << std::setfill('0') << v << ' '; };
    for(auto v:scalars)word(v);
    for(auto v:request)word(v);
    for(auto v:c.config)word(v);
    for(auto v:expected)word(v);
    vectors << '\\n';
    ++compared;''')
(out / 'reference.cpp').write_text(source)
subprocess.run(['cmake', '--build', str(experiment / 'build'), '--target', 'dmotor', '-j2'], check=True)
subprocess.run(['g++','-std=c++14','-fsanitize=undefined','-fno-sanitize-recover=all',
    '-I'+str(experiment/'include'),str(out/'reference.cpp'),str(experiment/'build/libdmotor.a'),
    '-o',str(out/'reference')],check=True)
subprocess.run([str(out/'reference')],check=True,cwd=out)
cmd=[a.iverilog,'-g2012','-s','shared_engine_transport_tb','-o',str(out/'test.vvp')]
if a.base:cmd+=['-B',a.base]
cmd += [str(x) for x in sorted(generated.glob('*.v'))]
cmd += [str(root/x) for x in ['rtl/timing.sv','rtl/shared_engine_transport.sv','tests/shared_engine_transport_tb.sv']]
subprocess.run(cmd,check=True,cwd=generated)
subprocess.run([a.vvp,str(out/'test.vvp'),'+vectors='+str(out/'vectors.txt')],check=True,cwd=generated,timeout=600)
