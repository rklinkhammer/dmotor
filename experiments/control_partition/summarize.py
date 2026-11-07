#!/usr/bin/env python3
"""Summarize measured and simulated results without mixing their evidence levels."""
import json
import re
from pathlib import Path
root=Path(__file__).resolve().parents[2]
runs=json.loads((root/'build/partition/ps_results.json').read_text())['runs']
rows=[]
for tau in [.02,.1,.3]:
    for feedback in ['ideal','counts']:
        row={'tau_s':tau,'feedback':feedback}
        for hz in [1000,200,100]:
            x=next(x for x in runs if x['tau_s']==tau and x['feedback']==feedback
                   and x['hz']==hz and x['scenario']=='nominal')
            row[str(hz)+'hz_rms_mm_s']=round(x['wheel_tracking_rms_m_s']*1000,3)
        rows.append(row)
summary={'simulation_only':True,'nominal_tracking':rows,'scenarios_200hz_tau100ms':[
    x for x in runs if x['hz']==200 and x['tau_s']==.1 and x['feedback']=='ideal'],
    'synthesis':{}}
for name,report in [('baseline','build/opt/area/measurement/utilization.rpt'),
                    ('candidate','build/partition/measurement/utilization.rpt')]:
    p=root/report
    if p.exists():
        s=p.read_text();row={}
        for key,label in [('lut','Slice LUTs'),('ff','Slice Registers'),('dsp','DSPs'),('bram_tiles','Block RAM Tile')]:
            m=re.search(r'^\|\s*'+label+r'\*?\s*\|\s*([\d.]+)',s,re.M)
            if m:row[key]=float(m[1])
        summary['synthesis'][name]=row
out=root/'build/partition/summary.json'
out.write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary,indent=2))
