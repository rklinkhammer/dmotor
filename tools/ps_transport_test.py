#!/usr/bin/env python3
import argparse,subprocess
from pathlib import Path
p=argparse.ArgumentParser()
for name in ['iverilog','vvp','base','build']:p.add_argument('--'+name,required=True)
a=p.parse_args();r=Path(__file__).resolve().parents[1];out=Path(a.build)/'ps_transport.vvp'
cmd=[a.iverilog,'-g2012','-s','transport_tb','-o',str(out)]
if a.base:cmd+=['-B',a.base]
cmd += [str(r/n) for n in ['rtl/timing.sv','rtl/shared_engine_transport.sv','tests/ps_transport_tb.sv']]
subprocess.run(cmd,check=True);subprocess.run([a.vvp,str(out)],check=True,timeout=30)
