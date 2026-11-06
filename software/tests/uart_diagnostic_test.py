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

def encode(kind, run, seq):
    data = pattern(kind, seq)
    raw = struct.pack('<BBBQIB', 215, 1, kind, run, seq, len(data)) + data
    raw += struct.pack('>H', binascii.crc_hqx(raw, 65535))
    return b'~' + b''.join(bytes([125, c ^ 32]) if c in (125,126) else bytes([c]) for c in raw) + b'~'

def run_case(mode):
    master, slave = pty.openpty()
    proc = subprocess.Popen([sys.argv[1], os.ttyname(slave), '10'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
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
                        assert (magic, version, kind) == (215, 1, 1)
                        assert raw[16:-2] == pattern(1, seq) and len(raw) == size+18
                        packets += 1
                        response = encode(2, nonce, seq)
                        if mode == 'corrupt': response = response[:-2] + bytes([response[-2]^1]) + response[-1:]
                        if mode == 'stale': response = encode(2, nonce+1, seq)
                        if mode != 'silent':
                            # Split frames to force incremental receive, including escapes.
                            for i in range(0, len(response), 3): os.write(master, response[i:i+3])
                    raw.clear();escape = False
                elif c == 125 and not escape: escape = True
                else:
                    raw.append(c ^ 32 if escape else c);escape = False
        out, err = proc.communicate(timeout=2)
        assert (proc.returncode == 0) == (mode == 'good'), (mode, out, err)
        if mode == 'good': assert packets == 10 and b'verified=10' in out
        else: assert b'FAIL' in err
    finally:
        if proc.poll() is None: proc.kill();proc.wait()
        os.close(master);os.close(slave)
for mode in ['good', 'corrupt', 'stale', 'silent']: run_case(mode)
print('UART diagnostic PTY PASS: independent CRC/payload peer, fragmented I/O, corruption, stale run, absent replies')
