#!/usr/bin/env python3
"""Exercise the atomic mailbox adapter against actual generated HLS RTL."""
import argparse
from pathlib import Path
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--iverilog',default='iverilog')
p.add_argument('--vvp',default='vvp')
p.add_argument('--base',default='')
p.add_argument('--test', choices=['atomic_adapter', 'mailbox_control', 'controller_cleanup'], default='atomic_adapter')
p.add_argument('--engine-profile',type=Path,help='Use actual shared motor HLS for controller_cleanup instead of a transport stub')
a=p.parse_args()
r=Path(__file__).resolve().parents[1]
g=r/'build/vitis_atomic/hls/syn/verilog'
if not (g/'dmotor_atomic_mailbox.v').exists():
    raise SystemExit('Run tools/vitis_check.py --kernel atomic first')
simulation_dir=g
cmd=[a.iverilog,'-g2012','-s',a.test+'_tb','-o',str(r/('build/'+a.test+'.vvp'))]
if a.base:cmd+=['-B',a.base]
cmd += [str(x) for x in sorted(g.glob('*.v'))]
cmd += [str(r/x) for x in ['rtl/timing.sv','rtl/batch_registers.sv','rtl/atomic_hls_adapter.sv',
    'rtl/atomic_mailbox_arbiter.sv','rtl/shared_scheduler.sv','rtl/target_age.sv','rtl/pwm.sv',
    'rtl/shared_control.sv','rtl/mailbox_control.sv','tests/'+a.test+'_tb.sv']]
if a.test == 'controller_cleanup':
    # Motor transport stub only; atomic mailbox remains actual generated HLS.
    cmd += [str(r/'rtl'/f'{name}.sv') for name in
            ['controller_registers','motor_engine_adapter','cleanup_sequencer',
             'integrated_control','safety','qcencoder','encoder_snapshot','motor_io',
             'monitor_heartbeat','board_status','axi_host','controller_axi']]
    if a.engine_profile:
        engine=a.engine_profile.resolve()/'build/vitis_dispatcher/hls/syn/verilog'
        if not (engine/'dmotor_mailbox_shared.v').is_file():
            raise SystemExit('Missing generated shared motor HLS')
        cmd += [str(x) for x in sorted(engine.glob('*.v'))]
        cmd += [str(r/'rtl/shared_engine_transport.sv')]
        # HLS ROM data are resolved relative to the simulator working directory.
        import shutil
        simulation_dir=r/'build/cleanup_actual_mem'
        simulation_dir.mkdir(exist_ok=True)
        for directory in (g,engine):
            for pattern in ('*.dat','*.mem'):
                for data in directory.glob(pattern):
                    destination=simulation_dir/data.name
                    if directory==engine and (g/data.name).exists() and (g/data.name).read_bytes()!=data.read_bytes():
                        raise SystemExit('HLS memory filename collision: '+data.name)
                    shutil.copyfile(data,destination)
    else:
        cmd += [str(r/'tests/motor_engine_adapter_tb.sv')]
subprocess.run(cmd,check=True,cwd=g)
subprocess.run([a.vvp,str(r/('build/'+a.test+'.vvp'))],check=True,cwd=simulation_dir,timeout=60)
