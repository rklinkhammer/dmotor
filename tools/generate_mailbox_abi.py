#!/usr/bin/env python3
"""Generate the separate, provisional atomic mailbox register contract."""
import argparse
import json
from pathlib import Path
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--check', action='store_true')
a = p.parse_args()
r = Path(__file__).resolve().parents[1]
s = json.loads((r/'abi/mailbox.json').read_text())
assert s['motor_count'] == 4
values = {'PROFILE_ID': s['identity'], 'SCHEMA_VERSION': s['schema_version'],
          'CHANNELS': s['motor_count'], 'STAGE_MOTOR_BASE': s['stage_motor_base'],
          'STAGE_MOTOR_STRIDE': s['stage_motor_stride'],
          'APPLIED_MOTOR_BASE': s['applied_motor_base'], 'APPLIED_MOTOR_STRIDE': s['applied_motor_stride']}
seen = set()
for v in s['registers']:
    assert v['offset'] % 4 == 0 and v['offset'] not in seen
    assert v['access'] in ('RO','RW','WO')
    seen.add(v['offset'])
    values[v['name']] = v['offset']
for v in s['motor_fields']:
    values[v['name']] = v['offset']
    for motor in range(s['motor_count']):
        offset = s['stage_motor_base'] + motor*s['stage_motor_stride'] + v['offset']
        assert offset % 4 == 0 and offset not in seen
        seen.add(offset)
for name, value in s['reject_reasons'].items():
    values['REJECT_' + name.upper()] = value
cpp = ['// Generated from abi/mailbox.json; provisional, separate from ring ABI.', '#pragma once',
       '#include <cstdint>', 'namespace dmotor { namespace batch_reg {']
sv = ['// Generated from abi/mailbox.json; provisional.', 'package dmotor_batch_registers;']
for name, value in values.items():
    cpp.append(f'constexpr uint32_t {name} = {value}U;')
    sv.append(f'localparam logic [31:0] {name} = 32\'d{value};')
cpp.append('} }')
sv.append('endpackage')
md = ['# Atomic mailbox draft register map', '',
      'Distinct DMB1 identity; development schema 1. No legacy ring offsets are reinterpreted.', '',
      '| Name | Offset | Access | Reset | Meaning |', '|---|---:|---|---:|---|']
for v in s['registers']:
    md.append(f"| {v['name']} | 0x{v['offset']:03x} | {v['access']} | {v['reset']} | {v['meaning']} |")
md += ['', 'Motor staging: base 0x080, stride 8 bytes, four channels.',
       'GENERATION is +0; signed VELOCITY_MRPM is +4. Unselected fields are ignored.',
       'Only aligned full-word writes are accepted. Action registers read zero.',
       'SESSION is supplied by trusted integration reset, never writable by software.',
       'Per-motor applied telemetry: base 0x100, stride 16; sequence +0, tick low +4, tick high +8. +12 reserved.',
       'Use the global applied-sequence/session validation around a multiword read.',
       'See atomic_mailbox.md for publication, activation and snapshot rules.', '',
       '| Rejection | Code |', '|---|---:|']
for name, value in s['reject_reasons'].items():
    md.append(f'| {name} | {value} |')
for name, lines in [('include/dmotor/batch_registers.hpp',cpp),('rtl/batch_registers.sv',sv),('docs/batch_registers.md',md)]:
    path = r/name
    data = '\n'.join(lines)+'\n'
    if a.check:
        if not path.exists() or path.read_text()!=data: raise SystemExit(f'Stale generated file: {path}')
    else: path.write_text(data)
