#!/usr/bin/env python3
"""Build the default 1 kHz PSF1 controller and matched PS software; no device access."""
import argparse,fcntl,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/product'
BOARD_PLATFORMS=('arty-z7-20','dvrz','zybo-z7-20')
INPUTS=('src','include','rtl','tests','tools','platforms','abi','constraints','software')
def fingerprint(board=True):
 # Include new source files, but never generated ESP-IDF build/cache output.
 names=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard','--','CMakeLists.txt',*INPUTS],cwd=ROOT).decode().split('\0')
 paths=[ROOT/n for n in names if n and (ROOT/n).is_file()]
 digest=hashlib.sha256()
 for p in sorted(set(paths)):
  if p.suffix=='.md' or (not board and any((ROOT/'platforms'/n) in p.parents for n in BOARD_PLATFORMS)):continue
  digest.update(str(p.relative_to(ROOT)).encode()+b'\0'+p.read_bytes()+b'\0')
 return digest.hexdigest()
def packaged_hash():
 d=ROOT/'build/current_ip/controller_axi/ip'
 if not (d/'component.xml').is_file():return None
 h=hashlib.sha256()
 for p in sorted(x for x in d.rglob('*') if x.is_file()):h.update(str(p.relative_to(d)).encode()+b'\0'+p.read_bytes())
 return h.hexdigest()
def run(command,env=None):
 print('+ '+__import__('shlex').join(map(str,command)),flush=True)
 subprocess.run(list(map(str,command)),cwd=ROOT,env=env,check=True)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('stage',choices=['test','ip','project'])
 p.add_argument('--platform',choices=BOARD_PLATFORMS,default='arty-z7-20')
 p.add_argument('--amd-root',type=Path,default=Path('/opt/vitis/2026.1'))
 p.add_argument('--force',action='store_true');p.add_argument('--dry-run',action='store_true')
 p.add_argument('--bench',choices=['controller_axi','controller_cleanup'])
 a=p.parse_args();key={'source_sha256':fingerprint(board=False),'amd_root':str(a.amd_root.resolve()),'profile':'PSF1-1000us'}
 print(f'DMOTOR source: {ROOT}\nHardware fingerprint: {key["source_sha256"]}',flush=True)
 if a.dry_run:
  print('Stages: direct 1 ms product sources -> atomic HLS csim/synthesis/cosim -> native + RTL/PS co-simulation + PS software tests -> controller IP export'+(f' -> {a.platform} board project' if a.stage=='project' else ''));return
 if a.stage=='project' and a.platform=='zybo-z7-20':
  run([sys.executable,ROOT/'tools/check_zybo_interface.py','--require-pins'])
 local=ROOT/'build/tools/usr';iv=shutil.which('iverilog') or str(local/'bin/iverilog');vvp=shutil.which('vvp') or str(local/'bin/vvp')
 base=str(local/'lib/x86_64-linux-gnu/ivl') if iv==str(local/'bin/iverilog') else ''
 if not Path(iv).is_file() or not Path(vvp).is_file():p.error('Install Icarus Verilog including VPI development headers')
 OUT.mkdir(parents=True,exist_ok=True)
 with (ROOT/'build/.product.lock').open('a') as lock:
  fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
  build=OUT/'native';sw=OUT/'software';stamp=OUT/'ip.json'
  saved=json.loads(stamp.read_text()) if stamp.exists() else {}
  cached=not a.force and saved.get('inputs')==key and saved.get('packaged_sha256')==packaged_hash()
  if a.bench:
   run([sys.executable,ROOT/'tools/ps_cosim.py','--build',build,'--bench',a.bench,'--iverilog',iv,'--vvp',vvp,'--base',base]);return
  if not cached:
   stamp.unlink(missing_ok=True)
   for name in ['Vivado/bin/vivado','Vitis/bin/vitis-run','Vitis/bin/v++']:
    if not (a.amd_root/name).is_file():p.error('Missing AMD tool '+name)
   run([sys.executable,'tools/vitis_check.py','--kernel','atomic','--vitis',a.amd_root/'Vitis'])
   run(['cmake','-S',ROOT,'-B',build,'-DCMAKE_POSITION_INDEPENDENT_CODE=ON','-DIVERILOG='+iv,'-DVVP='+vvp,'-DIVERILOG_BASE='+base])
   run(['cmake','--build',build,'-j4'])
   run(['ctest','--test-dir',build,'--output-on-failure'])
   run(['cmake','-S',ROOT/'software','-B',sw,'-DDMOTOR_SW_SANITIZE=ON'])
   run(['cmake','--build',sw,'-j4'])
   run(['ctest','--test-dir',sw,'--output-on-failure'])
   run([a.amd_root/'Vivado/bin/vivado','-mode','batch','-source','platforms/vivado/export_current.tcl'])
   if fingerprint(board=False)!=key['source_sha256']:raise RuntimeError('Sources changed during build; rerun before exporting a matched release')
   stamp.write_text(json.dumps({'inputs':key,'packaged_sha256':packaged_hash()},indent=2)+'\n')
  else:print('Reusing fingerprint-matched tested 1 ms PSF1 IP/software.',flush=True)
  if a.stage=='project':
   project=OUT/'vivado' if a.platform=='arty-z7-20' else OUT/a.platform/('release-vivado' if a.platform=='zybo-z7-20' else 'vivado')
   if project.exists():shutil.rmtree(project)
   run([a.amd_root/'Vivado/bin/vivado','-mode','batch','-source',f'platforms/{a.platform}/vivado/create.tcl','-tclargs',project]+(['build'] if a.platform=='zybo-z7-20' else []))
  print('Complete: '+str(OUT),flush=True)
if __name__=='__main__':
 try:main()
 except (OSError,ValueError,RuntimeError,subprocess.CalledProcessError) as e:sys.exit(str(e))
