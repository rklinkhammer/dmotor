#!/usr/bin/env python3
"""Package PSF1 only inside its prepared profile; optionally recreate original board BD."""
import argparse,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',type=Path,required=True)
p.add_argument('--platform',choices=['arty-z7-20','dvrz']);p.add_argument('--vivado',default='/opt/vitis/2026.1/Vivado/bin/vivado');a=p.parse_args()
r=Path(__file__).resolve().parents[2];profile=a.profile.resolve()
if not (profile/'ps_profile.json').is_file():p.error('Prepared PS profile required')
out=profile/'build';out.mkdir(exist_ok=True)
source=(r/'platforms/vivado/export_current.tcl').read_text()
proc=source[source.index('proc export_block'):source.index('\nexport_block $root $out mailbox_control')]
proc=proc.replace('set_property core_revision 11','set_property core_revision 12').replace('DMOTOR current','DMOTOR PSF1 parallel').replace('Current tested component. Not a complete motor controller; see docs/current_ip_export.md.','Parallel PS executor profile; requires PSF1 daemon/driver; no hardware qualification.')
files='timing controller_registers batch_registers atomic_hls_adapter atomic_mailbox_arbiter shared_scheduler target_age pwm shared_control mailbox_control shared_engine_transport motor_engine_adapter cleanup_sequencer integrated_control safety qcencoder encoder_snapshot motor_io monitor_heartbeat axi_host controller_axi'.split()
tcl=out/'package_ps.tcl'
tcl.write_text('set root {'+str(profile)+'}\nset out [file join $root build current_ip]\nfile mkdir $out\n'+proc+'\nexport_block $root $out controller_axi dmotor_controller_axi [list {'+str(r/'build/vitis_atomic/hls/syn/verilog')+'}] {'+' '.join(n+'.sv' for n in files)+'}\nputs "PS_PARALLEL_PACKAGE_PASS"\n')
with (out/'package.txt').open('w') as f:subprocess.run([a.vivado,'-mode','batch','-nojournal','-nolog','-source',str(tcl)],stdout=f,stderr=subprocess.STDOUT,check=True)
if a.platform:
 with (out/(a.platform+'_project.txt')).open('w') as f:
  subprocess.run([a.vivado,'-mode','batch','-nojournal','-nolog','-source',str(profile/f'platforms/{a.platform}/vivado/create.tcl'),'-tclargs',str(out/a.platform/'vivado')],stdout=f,stderr=subprocess.STDOUT,check=True)
print('Packaged isolated PSF1 controller'+(' and recreated '+a.platform if a.platform else ''))
