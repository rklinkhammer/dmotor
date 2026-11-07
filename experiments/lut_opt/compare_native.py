#!/usr/bin/env python3
"""Compare every emitted service reply against an independently built old profile."""
import argparse, hashlib, subprocess
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--baseline',type=Path,required=True)
p.add_argument('--candidate',type=Path,required=True)
a=p.parse_args()
r=Path(__file__).resolve().parents[2]
out=r/'build/opt/native_equivalence';out.mkdir(parents=True,exist_ok=True)
traces=[]
for label,profile in [('baseline',a.baseline.resolve()),('candidate',a.candidate.resolve())]:
 source=(profile/'tests/shared_mailbox_test.cpp').read_text()
 marker='    ++compared;'
 if source.count(marker)!=1:raise SystemExit('Review changed test instrumentation point')
 source=source.replace(marker, '''    std::cout << "TRACE";
    for (unsigned word=0; word<16; ++word) std::cout << ' ' << c.reply[word];
    std::cout << '\\n';
    ++compared;''')
 src=out/(label+'.cpp');src.write_text(source)
 exe=out/label
 subprocess.run(['g++','-std=c++14','-fsanitize=undefined','-fno-sanitize-recover=all',
                 '-I'+str(profile/'include'),str(src),str(profile/'build/libdmotor.a'),'-o',str(exe)],check=True)
 traces.append(subprocess.check_output([str(exe)]))
 (out/(label+'.txt')).write_bytes(traces[-1])
if traces[0]!=traces[1]:raise SystemExit('Baseline and candidate reply traces differ')
print('NATIVE_REPLY_EQUIVALENCE_PASS:',traces[0].count(b'TRACE'),'services, all 16 words')
print('SHA256:',hashlib.sha256(traces[0]).hexdigest())
