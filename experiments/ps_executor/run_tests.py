#!/usr/bin/env python3
"""Build native C++14/UBSan and real-AXI RTL tests; no hardware or daemon access."""
import argparse
from pathlib import Path
import shutil
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--iverilog',type=Path)
p.add_argument('--vvp',type=Path)
p.add_argument('--ivl-base',type=Path)
a=p.parse_args()
r=Path(__file__).resolve().parents[2];out=r/'build/ps_executor';out.mkdir(parents=True,exist_ok=True)
for test in ['executor','transport','plant']:
 exe=out/(test+'_test')
 subprocess.run(['g++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=undefined',
                 '-fno-sanitize-recover=all',str(r/f'experiments/ps_executor/{test}_test.cpp'),'-o',str(exe)],check=True)
 result=subprocess.check_output([str(exe)],text=True)
 (out/(test+'_test.txt')).write_text(result);print(result,end='')
local=r/'build/opt/tools/root/usr'
iv=a.iverilog or shutil.which('iverilog') or local/'bin/iverilog'
vvp=a.vvp or shutil.which('vvp') or local/'bin/vvp'
cmd=[str(iv)]
base=a.ivl_base or (local/'lib/x86_64-linux-gnu/ivl' if str(iv)==str(local/'bin/iverilog') else None)
if base:cmd+=['-B',str(base)]
cmd+=['-g2012','-s','ps_controller_tb','-o',str(out/'rtl_test')]
cmd += [str(r/f'rtl/{name}.sv') for name in ['timing','axi_host','qcencoder','encoder_snapshot','pwm','monitor_heartbeat']]
cmd += [str(r/f'experiments/ps_executor/{name}.sv') for name in ['ps_controller_axi','ps_controller_tb']]
subprocess.run(cmd,check=True)
run=subprocess.run([str(vvp),str(out/'rtl_test')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
result=run.stdout
(out/'rtl_test.txt').write_text(result);print(result,end='')
run.check_returncode()
assert 'PASS: AXI four-wheel' in result
