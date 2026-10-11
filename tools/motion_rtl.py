#!/usr/bin/env python3
"""Simulate the actual synthesized HLS motion with the scheduler and PWM."""
import argparse
from pathlib import Path
import subprocess
p=argparse.ArgumentParser()
p.add_argument('--iverilog',default='iverilog')
p.add_argument('--vvp',default='vvp')
p.add_argument('--base',default='')
a=p.parse_args()
r=Path(__file__).resolve().parents[1]
g=r/'build/vitis_motion/hls/syn/verilog'
if not (g/'dmotor_motion.v').exists():
    raise SystemExit('Run tools/vitis_check.py --kernel motion first')
cmd=[a.iverilog,'-g2012','-s','motion_top_tb','-o',str(r/'build/motion_top.vvp')]
if a.base:cmd+=['-B',a.base]
cmd += [str(x) for x in sorted(g.glob('*.v'))]
cmd += [str(r/x) for x in ['rtl/scheduler.sv','rtl/pwm.sv','rtl/motion_top.sv','tests/motion_top_tb.sv']]
subprocess.run(cmd,check=True,cwd=g)
subprocess.run([a.vvp,str(r/'build/motion_top.vvp')],check=True,cwd=g,timeout=180)
