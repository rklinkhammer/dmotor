#!/usr/bin/env python3
"""Compile/run an isolated RTL test. Tool absence is never reported as a pass."""
import argparse
import pathlib
import subprocess
p = argparse.ArgumentParser()
for name in ('iverilog', 'vvp', 'base', 'source', 'build', 'test'):
    p.add_argument('--' + name, required=True)
a = p.parse_args()
root = pathlib.Path(a.source)
output = pathlib.Path(a.build) / (a.test + '.vvp')
command = [a.iverilog, '-g2012', '-Wall', '-s', a.test + '_tb', '-o', str(output)]
if a.base:
    command += ['-B', a.base]
command += [str(root / 'rtl/timing.sv')]
if a.test == 'shared_scheduler':
    command += [str(root / 'rtl/target_age.sv'), str(root / 'rtl/pwm.sv'), str(root / 'rtl/shared_control.sv')]
if a.test == 'target_age':
    command += [str(root / 'rtl/safety.sv')]
command += [str(root / 'rtl' / (('safety' if a.test == 'latency' else a.test) + '.sv')), str(root / 'tests' / (a.test + '_tb.sv'))]
subprocess.run(command, check=True)
subprocess.run([a.vvp, str(output)], check=True, timeout=30)
