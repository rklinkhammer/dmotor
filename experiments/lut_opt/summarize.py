#!/usr/bin/env python3
"""Print matched Vivado measurements without conflating HLS and implementation."""
from pathlib import Path
import json,re
root=Path(__file__).resolve().parents[2]
rows={}
for name in ('baseline','counters','decoders','bram','lutram','area','compact','compact_lutram','compact_registers'):
 p=root/'build/opt'/name/'measurement/utilization.rpt'
 if not p.exists():continue
 s=p.read_text()
 row={}
 for key,label in [('lut','Slice LUTs'),('ff','Slice Registers'),('dsp','DSPs'),('bram_tiles','Block RAM Tile')]:
  match=re.search(r'^\|\s*'+re.escape(label)+r'\*?\s*\|\s*([\d.]+)',s,re.M)
  if match:row[key]=float(match[1]) if '.' in match[1] else int(match[1])
 if 'lut' in row and 'ff' in row:rows[name]=row
if 'baseline' in rows:
 for row in rows.values():
  row['lut_delta']=row['lut']-rows['baseline']['lut']
  row['ff_delta']=row['ff']-rows['baseline']['ff']
print(json.dumps(rows,indent=2))
