#!/usr/bin/env python3
"""Exercise the atomic mailbox adapter against actual generated HLS RTL."""
import argparse
from pathlib import Path
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--iverilog',default='iverilog')
p.add_argument('--vvp',default='vvp')
p.add_argument('--base',default='')
p.add_argument('--test', choices=['atomic_adapter', 'mailbox_control'], default='atomic_adapter')
a=p.parse_args()
r=Path(__file__).resolve().parents[1]
g=r/'build/vitis_atomic/hls/syn/verilog'
if not (g/'dmotor_atomic_mailbox.v').exists():
    raise SystemExit('Run tools/vitis_check.py --kernel atomic first')
cmd=[a.iverilog,'-g2012','-s',a.test+'_tb','-o',str(r/('build/'+a.test+'.vvp'))]
if a.base:cmd+=['-B',a.base]
cmd += [str(x) for x in sorted(g.glob('*.v'))]
cmd += [str(r/x) for x in ['rtl/timing.sv','rtl/batch_registers.sv','rtl/atomic_hls_adapter.sv',
    'rtl/atomic_mailbox_arbiter.sv','rtl/shared_scheduler.sv','rtl/target_age.sv','rtl/pwm.sv',
    'rtl/shared_control.sv','rtl/mailbox_control.sv','tests/'+a.test+'_tb.sv']]
subprocess.run(cmd,check=True,cwd=g)
subprocess.run([a.vvp,str(r/('build/'+a.test+'.vvp'))],check=True,cwd=g,timeout=60)
