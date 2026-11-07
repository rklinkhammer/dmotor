from pathlib import Path
import shutil,json,hashlib,shlex
w=Path('/state/builds/arty-z7-20/tmp/work');out=Path('/evidence')
s=next(w.glob('*/dmotor-semantic-tests/1.0'));d=next(w.glob('*/dmotor-access/1.0'))
for src,name in [(s/'temp/log.do_semantics','arm_semantics.log'),(s/'temp/run.do_semantics','arm_semantics_command.txt'),(d/'temp/log.do_compile','arm_driver_compile.log'),(s/'dmotor-software/software/source-version.json','arm_source-version.json')]:shutil.copyfile(src,out/name)
commands=json.loads((s/'build/compile_commands.json').read_text())
selected=[c for c in commands if any(c['file'].endswith('/ps_'+n+'_test.cpp') for n in ['worker','cadence','fused'])]
assert len(selected)==3
for c in selected:assert '-DNDEBUG' not in shlex.split(c['command']),c
extra=[c for c in commands if c['file'].endswith(('/acquisition_test.cpp','/timing_cycle_test.cpp'))]
assert len(extra)==2
for c in extra:assert '-UNDEBUG' in shlex.split(c['command']),c
selected+=extra
(out/'arm_test_compile_flags.json').write_text(json.dumps(selected,indent=2)+'\n')
records={}
for name in ['dmotor-semantic-tests','dmotor-protocol-tests','dmotor-linux-tests','dmotor-runtime-tests','dmotor-monitor-tests','dmotor-ps-worker-tests','dmotor-ps-cadence-tests','dmotor-ps-fused-tests','esp32-tests/acquisition_test','esp32-tests/timing_cycle_test','dmotord','dmotor-uart-test']:
 b=(s/'build'/name).read_bytes();assert b[:4]==b'\x7fELF' and int.from_bytes(b[18:20],'little')==40,name
 records[name]={'machine':'ARM','sha256':hashlib.sha256(b).hexdigest()}
(out/'arm_binary_checks.json').write_text(json.dumps(records,indent=2)+'\n')
print((out/'arm_semantics.log').read_text())
print('PASS: ten ARM test executables, ARM daemon/diagnostic binaries; new test assertions enabled')

for recipe in ['dmotor-semantic-tests','linux-xlnx','u-boot-xlnx','device-tree','xilinx-bootbin','fsbl-firmware','dvrmc-firmware','edf-linux-disk-image']:
 logs=list(w.glob('*/'+recipe+'/*/temp/log.do_compile'))
 for i,p in enumerate(logs):shutil.copyfile(p,out/(recipe+'-compile-'+str(i)+'.log'))
 print(recipe, [str(p) for p in logs])
