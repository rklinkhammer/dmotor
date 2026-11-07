#!/usr/bin/env python3
"""Verify a prepared PS profile without opening devices or programming hardware."""
import argparse,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[2];profile=a.profile.resolve()
if not (profile/'ps_profile.json').exists():p.error('Run prepare.py into a fresh directory first')
def run(cmd,log=None):
 if log:
  with log.open('w') as f:subprocess.run(list(map(str,cmd)),stdout=f,stderr=subprocess.STDOUT,check=True)
 else:subprocess.run(list(map(str,cmd)),check=True)
run(['cmake','-S',profile,'-B',profile/'build','-DCMAKE_POSITION_INDEPENDENT_CODE=ON'])
run(['cmake','--build',profile/'build','-j4'])
# These three legacy tests require position/queued-motion opcodes absent from the
# shipping velocity-only product too. Do not label them passing in this profile.
run(['ctest','--test-dir',profile/'build','-E','^(protocol|motion_queue|position_error)$','--output-on-failure'],profile/'product_regression.txt')
run([sys.executable,r/'experiments/ps_parallel/native_tests.py','--profile',profile])
local=r/'build/opt/tools/root/usr';iv=local/'bin/iverilog';vvp=local/'bin/vvp'
run([iv,'-B',local/'lib/x86_64-linux-gnu/ivl','-g2012','-s','transport_tb','-o',profile/'transport.vvp',profile/'rtl/timing.sv',r/'experiments/ps_parallel/shared_engine_transport.sv',r/'experiments/ps_parallel/transport_tb.sv'])
run([vvp,profile/'transport.vvp'],profile/'transport_test.txt')
for bench in ['controller_axi','controller_cleanup']:
 run([sys.executable,r/'experiments/ps_parallel/cosim.py','--profile',profile,'--bench',bench])
run(['cmake','-S',profile/'software','-B',profile/'ps-build'])
run(['cmake','--build',profile/'ps-build','-j4'])
run(['ctest','--test-dir',profile/'ps-build','--output-on-failure'],profile/'software_tests.txt')
print('PS parallel verification PASS; no board qualification or wall-clock guarantee implied')
