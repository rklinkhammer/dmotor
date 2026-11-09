#!/usr/bin/env python3
"""Check saved motor/monitor wiring and explicit pin assignments without Vivado."""
import argparse
import json
import re
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--allow-unassigned', action='store_true',
                    help='Report missing pins as pending; not a release check')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
design = json.loads((root / 'platforms/arty-z7-20/vivado/design_1.bd').read_text())['design']
components = design['components']
nets = [set(net['ports']) for net in design['nets'].values()]
errors = []


def require(condition, message):
    if not condition:
        errors.append(message)


def connected(*ports):
    require(any(set(ports) <= net for net in nets), 'Missing connection: ' + ' -> '.join(ports))


def parameter(cell, name, default=None):
    return components[cell].get('parameters', {}).get(name, {}).get('value', default)


for channel in range(4):
    for signal in ('qa', 'qb'):
        connected(f'mc{channel}_{signal}', f'{signal}_concat/In{channel}')
        require(design['ports'][f'mc{channel}_{signal}']['direction'] == 'I', 'Encoder direction')
    for signal, bus in (('dir', 'direction'), ('pwm', 'pwm')):
        cell = f'mc{channel}_{signal}'
        connected(f'controller_axi_0/{bus}', f'{cell}/Din')
        connected(f'{cell}/Dout', cell)
        require(design['ports'][cell]['direction'] == 'O', f'{cell} must be output')
        require(parameter(cell, 'DIN_WIDTH') == '4', f'{cell} input width')
        for bound in ('DIN_FROM', 'DIN_TO'):
            require(parameter(cell, bound, '0') == str(channel), f'{cell} wrong channel selection')
for signal in ('qa', 'qb'):
    connected(f'{signal}_concat/dout', f'controller_axi_0/{signal}')
for external, internal, direction in (
    *[(p, 'controller_axi_0/'+p, 'I' if p=='maintenance_async' else 'O') for p in ('maintenance_async','led4_r','led4_g','led4_b','led5_r','led5_g','led5_b')],
    ('mon_hb', 'controller_axi_0/hb_async', 'I'),
    ('coast_req', 'controller_axi_0/coast_req', 'O'),
    ('mon_rx', 'axi_uartlite_0/rx', 'I'),
    ('mon_tx', 'axi_uartlite_0/tx', 'O'),
):
    connected(external, internal)
    require(design['ports'][external]['direction'] == direction, f'{external} direction')
# Board automation must not retain second owners of the SW0/LD4/LD5 pins.
for legacy in ('rgb_led', 'sws_2bits'):
    require(legacy not in design.get('interface_ports', {}), f'Duplicate board pin owner: {legacy}')
require(parameter('axi_gpio_0', 'USE_BOARD_FLOW') == 'false', 'GPIO0 board automation must be disabled')
require(parameter('axi_gpio_0', 'C_IS_DUAL') == '0', 'Legacy switch readback must be disabled')
connected('axi_gpio_3/gpio_io_o', 'controller_axi_0/halt_async')
connected('controller_axi_0/drive_enable', 'axi_gpio_3/gpio2_io_i')
require(parameter('axi_gpio_3', 'C_GPIO2_WIDTH') == '4', 'drive_enable GPIO width')
require(parameter('axi_gpio_3', 'C_ALL_INPUTS_2') == '1', 'drive_enable GPIO must be input')
for key, expected in (('C_BAUDRATE', '115200'), ('C_DATA_BITS', '8'), ('C_USE_PARITY', '0')):
    require(parameter('axi_uartlite_0', key) == expected, f'UART {key} must explicitly be {expected}')
require(any('controller_axi_0/physical_faults' in net and len(net) > 1 for net in nets),
        'physical_faults must retain a driven source')

# Active single-port PACKAGE_PIN constraints in this board's XDC.
assignments = {}
for line in (root / 'platforms/arty-z7-20/constraints/arty-z7-20.xdc').read_text().splitlines():
    if line.lstrip().startswith('#'):
        continue
    match = re.search(r'PACKAGE_PIN\s+(\w+).*get_ports\s+\{?\s*(\w+)\s*\}?\]', line)
    if match:
        pin, port = match.groups()
        require(port not in assignments, f'Duplicate assignment: {port}')
        require(pin not in assignments.values(), f'Duplicate package pin: {pin}')
        require(port in design['ports'], f'Constraint for absent BD port: {port}')
        require('LVCMOS33' in line, f'{port} must use LVCMOS33')
        assignments[port] = pin
pending = sorted(set(design['ports']) - assignments.keys())
if pending:
    if args.allow_unassigned:
        print('PENDING pin assignments (not release-ready): ' + ', '.join(pending))
    else:
        errors.append('Missing package pins: ' + ', '.join(pending))
if errors:
    raise SystemExit('\n'.join(errors))
print('PASS: saved four-motor wiring, monitor UART/HB/coast and internal GPIO')
print('Pins: ' + ', '.join(f'{port}={pin}' for port, pin in sorted(assignments.items())))
