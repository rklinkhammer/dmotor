#!/usr/bin/env python3
import argparse,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];profile=r;build=a.build.resolve();out=build/'ps_native';out.mkdir(exist_ok=True)
# Reuse every existing shared-engine reference assertion, but call the new PS
# record decoder/owned context implementation instead of the HLS top wrapper.
s=(profile/'tests/shared_mailbox_test.cpp').read_text()
s=s.replace('#include <cstdlib>','#include <cstdlib>\n#include "dmotor_sw/ps_engine.hpp"\nstatic dmotor_ps::Engine ps_engine;')
start=s.index('    dmotor_mailbox_shared(motor,reset,request,tick,c.authorized,stop,fault,c.session,')
end=s.index('c.config,c.reply);',start)+len('c.config,c.reply);')
s=s[:start]+'''    dmotor_ps::Record record{};
    record[0]=motor;record[1]=unsigned(reset)|(unsigned(tick)<<1)|(unsigned(c.authorized)<<2)|
      (unsigned(stop)<<3)|(unsigned(fault)<<4)|(unsigned(c.negative)<<5);
    record[2]=c.session;record[3]=uint32_t(c.now);record[4]=uint32_t(c.now>>32);
    record[5]=uint32_t(c.count);record[6]=uint32_t(c.count>>32);
    record[7]=uint32_t(c.edge);record[8]=uint32_t(c.edge>>32);record[9]=c.direction;
    for(unsigned i=0;i<4;++i)record[11+i]=request[i];
    for(unsigned i=0;i<20;++i)record[15+i]=c.config[i];
    auto ps_reply=ps_engine.execute(record);
    for(unsigned i=0;i<16;++i)c.reply[i]=ps_reply[i];
'''+s[end:]
(out/'equivalence.cpp').write_text(s)
for name,path in [('equivalence',out/'equivalence.cpp'),('worker',r/'tests/ps_worker_test.cpp'),('cadence',r/'tests/ps_cadence_test.cpp'),('fused',r/'tests/ps_fused_test.cpp')]:
 subprocess.run(['g++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(profile/'include'),'-I'+str(r/'software/include'),str(path),str(build/'libdmotor.a'),'-o',str(out/name)],check=True)
 run=subprocess.run([str(out/name)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 (out/(name+'.txt')).write_text(run.stdout);print(run.stdout);run.check_returncode()

subprocess.run(['g++','-std=c++14','-Wall','-Wextra','-Werror','-pthread','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(profile/'include'),'-I'+str(r/'software/include'),'-I'+str(r/'software/include'),str(r/'software/linux/ps_device.cpp'),str(r/'tests/ps_backend_test.cpp'),str(build/'libdmotor.a'),'-o',str(out/'backend')],check=True)
run=subprocess.run([str(out/'backend')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(out/'backend.txt').write_text(run.stdout);print(run.stdout);run.check_returncode()
