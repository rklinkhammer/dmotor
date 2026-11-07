"""Independent wire peer tests: only protocol tests bypass the client library."""
import os
import socket
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def reply(schema=1, okay=1, message=b''):
    # Include extreme signed positions and deliberately nonzero fields on every
    # reply to check that the library exposes observations only for status().
    return struct.pack('<IIIIiiiiIIIIIIII128s', schema, okay, 0x1234, 42,
                       -1234, 0, 0, 0, 0, 0xffffffff, 0xffffffff, 0,
                       0x80000000, 0xffffffff, 0x7fffffff, 0, message)


with tempfile.TemporaryDirectory(prefix='dmotor-client-') as directory:
    path = str(Path(directory) / 'control.sock')
    result = subprocess.run([sys.argv[1], path, 'connect-fail'], timeout=3)
    require(result.returncode == 0, 'missing endpoint was not rejected')
    for mode in ('exercise', 'drop', 'refuse', 'timeout', 'schema', 'short', 'oversize',
                 'message', 'okay', 'disconnect', 'recovery', 'recovery-old',
                 'recovery-invalid', 'recovery-inconsistent', 'recovery-short'):
        with socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET) as server:
            server.bind(path)
            server.listen(1)
            server.settimeout(3)
            child = subprocess.Popen([sys.argv[1], path, mode], stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE, text=True)
            try:
                peer, _ = server.accept()
                with peer:
                    peer.settimeout(3)
                    def receive(expected):
                        data = peer.recv(4096)
                        require(len(data) == 28, f'{mode}: invalid request size')
                        require(struct.unpack('<IIIiiii', data) == expected,
                                f'{mode}: unexpected request {data.hex()}')
                    receive((1, 0, 0, 0, 0, 0, 0))  # Constructor admission ping.
                    if mode in ('drop', 'refuse'):
                        peer.sendall(reply(okay=0) if mode == 'refuse' else reply())
                        require(peer.recv(4096) == b'', 'Client did not release ownership')
                        output, error = child.communicate(timeout=3)
                        require(child.returncode == 0, f'{mode}: {output} {error}')
                        continue
                    peer.sendall(reply())
                    if mode.startswith('recovery'):
                        receive((2, 12, 0, 0, 0, 0, 0))
                        if mode != 'recovery-old':
                            state = 99 if mode == 'recovery-invalid' else 4
                            admitted = 1 if mode == 'recovery-inconsistent' else 0
                            data = reply(schema=2) + struct.pack('<II', state, admitted)
                            peer.sendall(data[:-1] if mode == 'recovery-short' else data)
                        peer.close()
                        output, error = child.communicate(timeout=3)
                        require(child.returncode == 0, f'{mode}: {output} {error}')
                        continue
                    receive((1, 1, 0, 0, 0, 0, 0))
                    if mode == 'exercise':
                        peer.sendall(reply())
                        for op, mask, values in ((2, 0, (0, 0, 0, 0)),
                                                 (3, 3, (0, 0, 0, 0)),
                                                 (4, 3, (1000, -2000, 0, 0)),
                                                 (5, 3, (0, 0, 0, 0)),
                                                 (6, 3, (0, 0, 0, 0)),
                                                 (7, 0, (0, 0, 0, 0)),
                                                 (11, 0, (-32768, 0, 0, 0)),
                                                 (9, 0, (0, 0, 0, 0)),
                                                 (10, 0, (0, 0, 0, 0)),
                                                 (0, 0, (0, 0, 0, 0)),
                                                 (8, 0, (0, 0, 0, 0))):
                            receive((1, op, mask, *values))
                            if op == 2:
                                time.sleep(.3)
                            peer.sendall(reply(okay=0, message=b'Rejected') if op == 10 else reply())
                    elif mode == 'timeout':
                        time.sleep(.3)
                        # A timed-out call must close, not reuse this stream.
                        require(peer.recv(4096) == b'', 'timeout did not close connection')
                    elif mode == 'schema':
                        peer.sendall(reply(schema=2))
                    elif mode == 'short':
                        peer.sendall(reply()[:-1])
                    elif mode == 'oversize':
                        peer.sendall(reply() + b'x')
                    elif mode == 'message':
                        peer.sendall(reply(message=b'x' * 128))
                    elif mode == 'okay':
                        peer.sendall(reply(okay=2))
                    # disconnect mode closes without responding.
                output, error = child.communicate(timeout=3)
                require(child.returncode == 0, f'{mode}: {output} {error}')
            finally:
                if child.poll() is None:
                    child.kill()
                    child.wait()
                os.unlink(path)
print('Client operations, wire validation, rejection, deadlines and disconnect passed')
