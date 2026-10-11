#!/usr/bin/env python3
"""Simulate the actual synthesized HLS peripheral with the AXI bridge."""
import argparse
from pathlib import Path
import subprocess
p=argparse.ArgumentParser()
p.add_argument('--iverilog',default='iverilog')
p.add_argument('--vvp',default='vvp')
p.add_argument('--base',default='')
a=p.parse_args()
r=Path(__file__).resolve().parents[1]
g=r/'build/vitis_peripheral/hls/syn/verilog'
if not (g/'dmotor_peripheral.v').exists():
    raise SystemExit('Run tools/vitis_check.py --kernel peripheral first')
cmd=[a.iverilog,'-g2012','-s','peripheral_top_tb','-o',str(r/'build/peripheral_top.vvp')]
if a.base:cmd+=['-B',a.base]
cmd += [str(x) for x in sorted(g.glob('*.v'))]
cmd += [str(r/x) for x in ['rtl/timing.sv','rtl/axi_bridge.sv','rtl/safety.sv','rtl/peripheral_top.sv','tests/peripheral_top_tb.sv']]
subprocess.run(cmd,check=True,cwd=g)
subprocess.run([a.vvp,str(r/'build/peripheral_top.vvp')],check=True,cwd=g,timeout=60)
