#!/usr/bin/env python3
"""Compare the shared-mailbox entry point with an independently compiled baseline.
Uses the established boundary suite plus repeated mixed-channel runtime episodes.
Does not claim generic dmotor_motion/dispatcher equivalence.
"""
import argparse
import hashlib
from pathlib import Path
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--baseline',type=Path,required=True)
p.add_argument('--candidate',type=Path,required=True)
a=p.parse_args()
root=Path(__file__).resolve().parents[2]
out=root/'build/partition/extended';out.mkdir(parents=True,exist_ok=True)
source=(root/'experiments/mailbox/shared_mailbox_test.cpp').read_text()
marker='    std::cout << "shared mailbox: " << compared'
assert source.count(marker)==1
source=source.replace(marker,r'''
    for(unsigned episode=0;episode<5;++episode) {
        for(unsigned m=0;m<4;++m) {
            channels[m]=Channel{};
            channels[m].session=100+episode*4+m;
            channels[m].config[0]=200+episode*4+m;
            service(m,0,0,0,false,true);
        }
        for(unsigned t=0;t<110;++t)for(unsigned m=0;m<4;++m)tick(m);
        for(unsigned m=0;m<4;++m)publish(m,1,20000+1000*m);
        for(unsigned t=0;t<400;++t)for(unsigned k=0;k<4;++k) {
            unsigned m=(k+t)%4;auto& c=channels[m];
            c.count+=1+(t+m+episode)%3;c.edge=c.now+dmotor::timing::control_tick_cycles;
            if(t==50 || t==100 || t==150) {
                int32_t target=static_cast<int32_t>((episode+1)*(m+1)*7000);
                if(t==100){target=-target;c.negative=true;++c.direction;}
                if(t==150){c.negative=false;++c.direction;}
                publish(m,2+t/50,target);
            }
            if(t==200)service(m,0,0,0,false,false,true);
            if(t==250)publish(m,9,1000);
            if(t==330 && m==2)++c.session;
            if(t==350 && m==3)++c.config[0];
            if(t==375 && m==0)c.authorized=false;
            tick(m,false,t==300 && m==1);
        }
    }
'''+marker)
source=source.replace('    ++compared;',r'''    std::cout << "TRACE";
    for(unsigned i=0;i<16;++i) std::cout << ' ' << c.reply[i];
    std::cout << '\n';
    ++compared;''')
traces=[]
for name,profile in [('baseline',a.baseline.resolve()),('candidate',a.candidate.resolve())]:
 src=out/(name+'.cpp');src.write_text(source);exe=out/name
 subprocess.run(['g++','-std=c++14','-fsanitize=undefined','-fno-sanitize-recover=all',
                 '-I'+str(profile/'include'),str(src),str(profile/'build/libdmotor.a'),
                 '-o',str(exe)],check=True)
 trace=subprocess.check_output([str(exe)])
 (out/(name+'.txt')).write_bytes(trace);traces.append(trace)
assert traces[0]==traces[1], 'Independent baseline/candidate replies differ'
print('PASS:',traces[0].count(b'TRACE'),'services, all 16 reply words, independent baseline/candidate')
print('SHA256:',hashlib.sha256(traces[0]).hexdigest())
