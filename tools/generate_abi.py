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

# Timing units and budgets are shared by C++ and RTL. Fabric clock identity is
# still owned by CLOCK_HZ in registers.json; never duplicate it in timing.json.
timing = json.loads((root / 'abi/timing.json').read_text())
fabric = next(r['reset'] for r in schema['registers'] if r['name'] == 'CLOCK_HZ')
assert all(isinstance(v, int) and v > 0 for v in timing.values())
assert 1000000 <= fabric <= 100000000 and fabric % 1000000 == 0, 'Validated clock range: integer MHz from 1 to 100'
for prefix in ('zero_timeout', 'watchdog', 'pi_period'):
    assert timing[prefix + '_min_us'] <= timing[prefix + '_max_us']
assert all(fabric % timing[k] == 0 for k in ('control_hz', 'pwm_hz', 'encoder_max_events_hz'))
assert 1000000 % timing['control_hz'] == 0
assert timing['encoder_max_events_hz'] % timing['control_hz'] == 0
# Existing arithmetic-width proofs depend on this envelope. Changing the
# envelope requires a separate numeric review; lowering fabric Hz does not.
assert timing['control_hz'] == 1000
assert timing['encoder_max_events_hz'] <= 100000
assert timing['pi_period_max_us'] <= 100000
assert timing['settle_max_ticks'] <= 1000000
units = {'microseconds_per_second': 1000000}
derived = {
    'fabric_hz': fabric,
    'clocks_per_us': fabric // 1000000,
    'control_period_us': 1000000 // timing['control_hz'],
    'control_tick_cycles': fabric // timing['control_hz'],
    'pwm_period_cycles': fabric // timing['pwm_hz'],
    'control_deadline_cycles': timing['control_deadline_us'] * (fabric // 1000000),
    'external_halt_budget_cycles': timing['external_halt_budget_us'] * (fabric // 1000000),
    'encoder_min_edge_cycles': fabric // timing['encoder_max_events_hz'],
    'encoder_max_events_per_tick': timing['encoder_max_events_hz'] // timing['control_hz'],
}
for name in ('encoder_max_interval', 'zero_timeout_min', 'zero_timeout_max', 'acquisition_timeout_min'):
    derived[name + '_cycles'] = timing[name + '_us'] * (fabric // 1000000)
assert derived['control_deadline_cycles'] < derived['control_tick_cycles']
assert timing['external_halt_pipeline_max_cycles'] <= derived['external_halt_budget_cycles']
assert all(v <= 0xffffffff for v in derived.values())
constants = {**units, **timing, **derived}
assert all(v <= 0xffffffff for v in constants.values())
header = '// Generated by tools/generate_abi.py; edit abi/timing.json or CLOCK_HZ in abi/registers.json.'
tcpp = [header, '#pragma once', '#include <cstdint>', '#include "dmotor/registers.hpp"',
        'namespace dmotor { namespace timing {']
tsv = [header, 'package dmotor_timing;']
for name, value in constants.items():
    tcpp.append(f'constexpr uint32_t {name} = {value};')
    tsv.append(f'localparam integer {name.upper()} = {value};')
tcpp += ['static_assert(fabric_hz == reg::FABRIC_HZ, "Regenerate timing constants");',
         '} } // namespace dmotor::timing']
tsv.append('endpackage')
for path, lines in [('include/dmotor/timing.hpp', tcpp), ('rtl/timing.sv', tsv)]:
    output = '\n'.join(lines) + '\n'
    target = root / path
    if a.check:
        if not target.exists() or target.read_text() != output:
            raise SystemExit(f'Stale generated file: {path}')
    else:
        target.write_text(output)
# Keep the HLS clock constraint consistent with the reported fabric frequency.
for path in (root / 'platforms/vitis').glob('*.cfg'):
    original = path.read_text()
    updated = re.sub(r'^freqhz=\d+$', f'freqhz={fabric}', original, flags=re.MULTILINE)
    if a.check and original != updated:
        raise SystemExit(f'Stale HLS clock: {path}')
    if not a.check and original != updated:
        path.write_text(updated)
