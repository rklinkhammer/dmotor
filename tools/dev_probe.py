#!/usr/bin/env python3
"""Probe the development-only endpoint through a correctly configured Linux UIO map."""
import argparse
import mmap
import os
import ctypes

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('device', help='UIO device whose map0 is the DMOTOR development peripheral')
a = p.parse_args()
fd = os.open(a.device, os.O_RDWR | os.O_SYNC)
try:
    with mmap.mmap(fd, 4096, flags=mmap.MAP_SHARED,
                   prot=mmap.PROT_READ | mmap.PROT_WRITE) as regs:
        def read(offset):
            return ctypes.c_uint32.from_buffer(regs, offset).value
        if read(0) != 0x444D4431 or read(4) != 1:
            raise SystemExit('Not the DMD1 development endpoint; no writes performed')
        if read(8) != 1:
            raise SystemExit('Expected permanent-inhibit status')
        saved = read(0x10)
        try:
            ctypes.c_uint32.from_buffer(regs, 0x10).value = 0xA55A1234
            if read(0x10) != 0xA55A1234:
                raise SystemExit('Scratch readback failed')
        finally:
            ctypes.c_uint32.from_buffer(regs, 0x10).value = saved
        print(f'DMD1 v1: inhibited, clock={read(12)} Hz, cycles={read(24)}; scratch passed')
finally:
    os.close(fd)
