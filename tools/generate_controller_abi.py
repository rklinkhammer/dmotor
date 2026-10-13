#!/usr/bin/env python3
"""Generate the provisional integrated-controller register definitions."""
import argparse
import json
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--check',action='store_true')
a=p.parse_args()
r=Path(__file__).resolve().parents[1]
s=json.loads((r/'abi/controller.json').read_text())
values={'PROFILE_ID':s['identity'],**s['registers']}
outputs={
 r/'include/dmotor/controller_registers.hpp': '#pragma once\n#include <cstdint>\nnamespace dmotor { namespace controller_reg {\n'+''.join(f'constexpr uint32_t {k} = {v}U;\n' for k,v in values.items())+'} }\n',
 r/'rtl/controller_registers.sv': 'package dmotor_controller_registers;\n'+''.join(f'localparam logic [31:0] {k} = 32\'d{v};\n' for k,v in values.items())+'endpackage\n'}
for file,contents in outputs.items():
 if a.check:
  if not file.exists() or file.read_text()!=contents:raise SystemExit(f'Stale ABI: {file}')
 else:file.write_text(contents)
