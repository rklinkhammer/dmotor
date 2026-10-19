#!/usr/bin/env python3
"""Current 400 RPM product build. Experiments are not selected by directory age."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build/product'
BOARD_PLATFORMS = ('arty-z7-20', 'dvrz')
# Include transformation patches until the tested profile becomes direct sources.
INPUTS = ('src','include','rtl','tests','tools','platforms','abi','constraints','experiments/mailbox',
          'docs/evidence/velocity_only','docs/evidence/velocity_bounded',
          'docs/evidence/velocity_exact_shared','docs/evidence/velocity_exact_onephase')
def fingerprint(board=True):
    paths = [ROOT/'CMakeLists.txt']
    for directory in INPUTS:
        paths += [p for p in (ROOT/directory).rglob('*') if p.is_file() and '__pycache__' not in p.parts]
    digest=hashlib.sha256()
    for p in sorted(set(paths)):
        if p.suffix=='.md' or (not board and any((ROOT/'platforms'/name) in p.parents for name in BOARD_PLATFORMS)): continue
        digest.update(str(p.relative_to(ROOT)).encode()+b'\0'+p.read_bytes()+b'\0')
    return digest.hexdigest()
def packaged_hash():
    digest=hashlib.sha256()
    for name in ('controller_axi','mailbox_control','shared_engine'):
        directory=ROOT/'build/current_ip'/name/'ip'
        if not (directory/'component.xml').is_file(): return None
        for path in sorted(p for p in directory.rglob('*') if p.is_file()):
            digest.update(str(path.relative_to(ROOT/'build/current_ip')).encode()+b'\0'+path.read_bytes())
    return digest.hexdigest()
def run(command, env=None):
    print('+ '+__import__('shlex').join(map(str,command)),flush=True)
    subprocess.run(list(map(str,command)),cwd=ROOT,env=env,check=True)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('stage',choices=['ip','project'])
    p.add_argument('--platform',choices=BOARD_PLATFORMS,default='arty-z7-20')
    p.add_argument('--amd-root',type=Path,default=Path('/opt/vitis/2026.1'))
    p.add_argument('--force',action='store_true')
    p.add_argument('--dry-run',action='store_true')
    a=p.parse_args()
    key={'source_sha256':fingerprint(board=False),'amd_root':str(a.amd_root.resolve())}
    print(f'DMOTOR source: {ROOT}\nHardware fingerprint: {key["source_sha256"]}',flush=True)
    project = OUT/'vivado' if a.platform=='arty-z7-20' else OUT/a.platform/'vivado'
    if a.stage=='project': print(f'Platform: {a.platform} -> {project}',flush=True)
    if a.dry_run:
        print('Stages: prepare 400 RPM profile -> native tests -> HLS csim/synthesis/cosim (atomic + shared engine) -> IP export -> full AXI simulation'+(' -> recreate Vivado project' if a.stage=='project' else ''))
        return
    for tool in ['Vivado/bin/vivado','Vitis/bin/vitis-run','Vitis/bin/v++']:
        if not (a.amd_root/tool).is_file(): p.error(f'Missing AMD tool: {a.amd_root/tool}')
    OUT.mkdir(parents=True,exist_ok=True)
    with (ROOT/'build/.product.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        stamp=OUT/'ip.json'
        saved=json.loads(stamp.read_text()) if stamp.exists() else {}
        cached=(saved.get('inputs')==key and saved.get('packaged_sha256')==packaged_hash()
                and (OUT/'engine/platforms/vitis/dispatcher.cfg').is_file())
        profile=OUT/'engine'
        env={**os.environ,'DMOTOR_ENGINE_PROFILE':str(profile)}
        vivado=a.amd_root/'Vivado/bin/vivado'
        if a.force or not cached:
            stamp.unlink(missing_ok=True)
            if profile.exists(): shutil.rmtree(profile)
            run([sys.executable,'tools/prepare_shared_mailbox.py','--output',profile])
            run(['cmake','-S',profile,'-B',profile/'build'])
            run(['cmake','--build',profile/'build','-j4'])
            run(['ctest','--test-dir',profile/'build','-R','^(arithmetic|motion|velocity|controller|dispatcher|mailbox|shared_mailbox)$','--output-on-failure'])
            for script,kernel in [(ROOT/'tools/vitis_check.py','atomic'),(profile/'tools/vitis_check.py','dispatcher')]:
                run([sys.executable,script,'--kernel',kernel,'--vitis',a.amd_root/'Vitis'])
            run([vivado,'-mode','batch','-source','platforms/vivado/export_current.tcl'],env)
            run([vivado,'-mode','batch','-source','platforms/vivado/sim_controller_axi.tcl'],env)
            if fingerprint(board=False)!=key['source_sha256']: raise RuntimeError('Sources changed during build; retry')
            stamp.write_text(json.dumps({'inputs':key,'packaged_sha256':packaged_hash()},indent=2)+'\n')
        else: print('Reusing fingerprint-matched, tested IP.',flush=True)
        if a.stage=='project':
            if project.exists(): shutil.rmtree(project)
            run([vivado,'-mode','batch','-source',f'platforms/{a.platform}/vivado/create.tcl','-tclargs',project],env)
        print('Complete: '+str(OUT),flush=True)
if __name__=='__main__':
    try: main()
    except (OSError,ValueError,RuntimeError,subprocess.CalledProcessError) as e: sys.exit(str(e))
