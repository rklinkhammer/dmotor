from pathlib import Path
import os,subprocess,shlex,json,hashlib,time
r=Path('/home/rklinkhammer/workspace/dmotor');out=r/'build/merged_release';launcher=r.parent/'esp32-dev/esp32'
profiles=[('uart_u1','software/esp32/diagnostics/bench','sdkconfig.defaults'),('acquisition_link','software/esp32/build_check','sdkconfig.defaults'),('ina_1000hz','software/esp32/diagnostics/ina_timing','sdkconfig.defaults'),('ina_probe100','software/esp32/diagnostics/ina_timing','sdkconfig.probe100.defaults')]
results=[]
for name,rel,defaults in profiles:
 project=r/rel; dest=out/'esp32'/name
 if dest.exists():raise SystemExit('Fresh output directory required: '+str(dest))
 dest.mkdir(parents=True)
 command=['env','IDF_CCACHE_ENABLE=0','CCACHE_DISABLE=1','idf.py','-B',str(dest/'build'),'-D','SDKCONFIG='+str(dest/'sdkconfig'),'-D','SDKCONFIG_DEFAULTS='+str(project/defaults),'build']
 full=[str(launcher),'--project',str(project),'--target','esp32s3','shell','-c',shlex.join(command)]
 print('Building '+name,flush=True)
 with (out/(name+'.log')).open('w') as log:
  run=subprocess.run(full,cwd=r,stdout=log,stderr=subprocess.STDOUT)
 result={'profile':name,'project':rel,'command':full,'exit_code':run.returncode}
 if run.returncode==0:
  result['sha256']={str(p.relative_to(dest)):hashlib.sha256(p.read_bytes()).hexdigest() for p in dest.rglob('*') if p.is_file() and (p.suffix in ('.bin','.elf','.map') or p.name=='sdkconfig')}
 results.append(result);(out/'esp32_results.json').write_text(json.dumps(results,indent=2)+'\n')
 if run.returncode:raise SystemExit('Failed '+name+'; see log')
 print('PASS '+name,flush=True)
