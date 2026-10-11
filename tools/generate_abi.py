#!/usr/bin/env python3
"""Generate draft register constants; JSON is the sole source of offsets."""
import argparse
import json
import re
from pathlib import Path
p = argparse.ArgumentParser()
p.add_argument('--check', action='store_true')
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
schema = json.loads((root / 'abi/registers.json').read_text())
match = re.search(r'constexpr uint32_t ring_depth = ([0-9]+);',
                  (root / 'include/dmotor/ring_config.hpp').read_text())
if not match:
    raise SystemExit('ring_config.hpp must define ring_depth as a decimal constexpr uint32_t')
ring_depth = int(match.group(1))
if not (2 <= ring_depth <= 64 and ring_depth & (ring_depth - 1) == 0):
    raise SystemExit('ring_depth must be a power of two from 2 to 64')
for name in ('submission', 'completion'):
    if schema['regions'][name]['slots'] != 'ring_depth':
        raise SystemExit(f'{name} slots must reference ring_depth, not a numeric duplicate')
seen = set()
cpp = ['// Generated; edit abi/registers.json or include/dmotor/ring_config.hpp.', '#pragma once', '#include <cstdint>', '#include "dmotor/ring_config.hpp"', 'namespace dmotor { namespace reg {']
sv = ['// Generated; edit abi/registers.json or include/dmotor/ring_config.hpp.', 'package dmotor_registers;']
md = ['# Provisional registers', '', 'Development schema; ring registers implemented, motion/safety configuration pending.', '', '| Register | Offset | Access | Reset | Meaning |', '|---|---|---|---|---|']
for r in schema['registers']:
    offset = r['offset']
    assert offset % 4 == 0 and offset not in seen
    assert r['access'] in ('RO', 'RW', 'WO')
    seen.add(offset)
    cpp.append(f'constexpr uint32_t {r["name"]} = 0x{offset:04x};')
    sv.append(f'localparam logic [15:0] {r["name"]} = 16\'h{offset:04x};')
    md.append(f'| {r["name"]} | 0x{offset:04x} | {r["access"]} | {r["reset"]} | {r["meaning"]} |')
for name, region in schema['regions'].items():
    for field in ('offset', 'slots', 'stride'):
        key = name.upper() + '_' + field.upper()
        value = ring_depth if region[field] == 'ring_depth' else region[field]
        cpp_value = 'dmotor::ring_depth' if region[field] == 'ring_depth' else value
        cpp.append(f'constexpr uint32_t {key} = {cpp_value};')
        sv.append(f'localparam integer {key} = {value};')
for name, value in [('ABI_SCHEMA', schema['schema_version']), ('FABRIC_HZ', next(r['reset'] for r in schema['registers'] if r['name'] == 'CLOCK_HZ'))]:
    cpp.append(f'constexpr uint32_t {name} = {value};')
    sv.append(f'localparam integer {name} = {value};')
md += ['', f'Submission and completion capacity: **{ring_depth} entries each**.',
       'Depth is defined in `include/dmotor/ring_config.hpp`; regenerate after changing it.']
cpp.append('} } // namespace dmotor::reg')
sv.append('endpackage')
for path, lines in [('include/dmotor/registers.hpp', cpp), ('rtl/registers.sv', sv), ('docs/registers.md', md)]:
    text = '\n'.join(lines) + '\n'
    target = root / path
    if a.check:
        if not target.exists() or target.read_text() != text:
            raise SystemExit(f'Stale generated file: {path}')
    else:
        target.write_text(text)
