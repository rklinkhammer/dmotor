#!/usr/bin/env python3
"""Independent Python peer exercises the installed-style Linux tty utility."""
import binascii
import errno
import os
import pty
import select
import struct
import subprocess
import sys
import time

def pattern(kind, seq):
    n = [0, 1, 2, 16, 64][(seq-1) % 5]
    return bytes([([0, 255, 125, 126][(i+kind-1) % 4] if i < 4 else (seq*17+i*31+kind*73) & 255) for i in range(n)])

def encode(nonce, seq, mode):
    boot = 0 if mode == 'zero_boot' else 124 if mode == 'restart' and seq > 1 else 123
    cycle = 1 if mode == 'repeat' else seq
    age = 60000 if mode == 'old' else 1000
    status = 1 if mode == 'failed' else 0
    payload = struct.pack('<QIIIIIIBBBB', boot,cycle,11000 if mode == 'duration' else 7697,age,int(mode=='failure'),int(mode=='skipped'),int(mode=='expiry'),status,11 if mode == 'reads' else 12,0x30 if mode == 'address' else 0x44,2 if mode in ('synthetic','mode') else 1 if mode == 'flags' else 0)
    payload += struct.pack('<14H', *([0x7e7d]*12+[600,75]))
    raw = struct.pack('<BBBQIB', 216,1,3 if mode == 'envelope' else 2,nonce,seq,64)+payload
    raw += struct.pack('>H',binascii.crc_hqx(raw,65535))
    return b'~'+b''.join(bytes([125,c^32]) if c in (125,126) else bytes([c]) for c in raw)+b'~'

def run_case(mode):
    master, slave = pty.openpty()
    proc = subprocess.Popen([sys.argv[1], os.ttyname(slave), '10'] + (['--synthetic'] if mode == 'synthetic' else []), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    raw = bytearray(); escape = False; packets = 0
    limit = time.monotonic() + 5
    try:
        while proc.poll() is None and time.monotonic() < limit:
            if not select.select([master], [], [], .05)[0]:
                continue
            try:
                data = os.read(master, 256)
            except OSError as e:
                if e.errno == errno.EIO: continue
                raise
            for c in data:
                if c == 126:
                    if raw:
                        assert binascii.crc_hqx(raw[:-2], 65535) == int.from_bytes(raw[-2:], 'big')
                        magic, version, kind, nonce, seq, size = struct.unpack('<BBBQIB', raw[:16])
                        assert (magic, version, kind) == (216, 1, 1)
                        assert raw[16:-2] == b'' and len(raw) == size+18
                        packets += 1
                        response = encode(nonce, seq, mode)
                        if mode == 'corrupt': response = response[:-2] + bytes([response[-2]^1]) + response[-1:]
                        if mode == 'stale': response = encode(nonce+1, seq, mode)
                        if mode == 'sequence': response = encode(nonce, seq+1, mode)
                        if mode != 'silent':
                            # Split frames to force incremental receive, including escapes.
                            for i in range(0, len(response), 3): os.write(master, response[i:i+3])
                    raw.clear();escape = False
                elif c == 125 and not escape: escape = True
                else:
                    raw.append(c ^ 32 if escape else c);escape = False
        out, err = proc.communicate(timeout=2)
        assert (proc.returncode == 0) == (mode in ('good','synthetic')), (mode, out, err)
        if mode in ('good','synthetic'): assert packets == 10 and b'verified=10' in out
        else:
            assert b'FAIL' in err
            reasons={'stale':'challenge','restart':'boot','repeat':'cycle','old':'age',
                     'failed':'sample','sequence':'sequence','failure':'failure',
                     'skipped':'skipped','expiry':'expiry','reads':'reads','address':'address',
                     'flags':'flags','duration':'duration','zero_boot':'boot','envelope':'envelope','mode':'mode'}
            if mode in reasons:
                assert ('Sample rejected: '+reasons[mode]).encode() in err, (mode,err)
                assert b'Response/TX timeout' not in err
    finally:
        if proc.poll() is None: proc.kill();proc.wait()
        os.close(master);os.close(slave)
for mode in ['good', 'corrupt', 'stale', 'silent', 'restart', 'repeat', 'old', 'failed', 'synthetic', 'sequence', 'failure', 'skipped', 'expiry', 'reads', 'address', 'flags', 'duration', 'zero_boot', 'envelope', 'mode']: run_case(mode)
print('Acquisition diagnostic PTY PASS: fragmented frames, corruption, challenge, timeout, restart, repeated/old/failed samples')
