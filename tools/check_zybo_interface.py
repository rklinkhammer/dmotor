#!/usr/bin/env python3
"""Check the imported Zybo scaffold; this is not a release/wiring acceptance gate."""
import argparse
import json
import re
from pathlib import Path
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--require-pins', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
base = root / 'platforms/zybo-z7-20'
d = json.loads((base / 'vivado/design_1.bd').read_text())['design']
assert d['design_info']['device'] == 'xc7z020clg400-1'
p = d['components']['processing_system7_0']['parameters']
assert abs(float(p['PCW_CRYSTAL_PERIPHERAL_FREQMHZ']['value']) - 33.333333) < 0.000001, 'Zybo PS reference must be 33.333333 MHz, not the Arty 50 MHz setting'
assert p['PCW_DDR_RAM_HIGHADDR']['value'] == '0x3FFFFFFF'
assert p['PCW_FPGA1_PERIPHERAL_FREQMHZ']['value'] == '20'
for m in range(4):
    for signal in ('dir', 'pwm', 'qa', 'qb'):
        assert d['ports'][f'mc{m}_{signal}']['direction'] == ('O' if signal in ('dir', 'pwm') else 'I')
assert d['ports']['maintenance_async']['direction'] == 'I'
logical_ports = set(d['ports'])
for name, interface in d.get('interface_ports', {}).items():
    if name not in ('DDR', 'FIXED_IO'):
        logical_ports.update(mapping['physical_name'] for mapping in interface.get('port_maps', {}).values())
ports, pins = {}, set()
for line in (base / 'constraints/zybo-z7-20.xdc').read_text().splitlines():
    if line.lstrip().startswith('#'):
        continue
    match = re.search(r'PACKAGE_PIN\s+(\w+).*get_ports\s+\{?\s*(\w+)\s*\}?\]', line)
    if not match:
        continue
    pin, port = match.groups()
    assert port in logical_ports, f'Constraint for absent port: {port}'
    assert port not in ports, f'Duplicate assignment: {port}'
    assert pin not in pins, f'Duplicate package pin: {pin}'
    assert 'LVCMOS33' in line
    ports[port] = pin
    pins.add(pin)
assert ports['mon_hb'] == 'T15' and ports['coast_req'] == 'T14'
print('PASS Zybo scaffold: Z7-20, 1 GiB DDR, 20 MHz controller clock, port directions and unique explicit pins')
missing = sorted(logical_ports - ports.keys())
print('PENDING pin assignments:', ', '.join(missing) or 'none')
if args.require_pins and missing:
    parser.error('Assign FPGA pins before hardware build: ' + ', '.join(missing))
print('Controller connections are checked separately by Vivado build-mode validation')
