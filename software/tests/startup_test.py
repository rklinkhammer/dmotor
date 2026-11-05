"""Reject invalid fixture paths before socket access; no physical device backend."""
import os
from pathlib import Path
import subprocess
import sys

base_env = dict(os.environ)
base_env.pop('DMOTOR_TEST_DIRECTORY', None)
config = Path(__file__).resolve().parents[1] / 'config/inhibited-example.conf'
for directory, message in ((None, 'Missing DMOTOR_TEST_DIRECTORY'),
                           ('', 'Missing DMOTOR_TEST_DIRECTORY'),
                           ('/tmp/' + 'x' * 200, 'Socket path too long')):
    env = dict(base_env)
    if directory is not None:
        env['DMOTOR_TEST_DIRECTORY'] = directory
    for command in ([sys.argv[1], '--externally-inhibited', '/dev/test-only', str(config)],
                    [sys.argv[2]]):
        result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=5)
        if result.returncode != 1 or message not in result.stderr:
            raise RuntimeError(f'{command}: expected clean rejection, got {result.returncode}: {result.stderr}')
for executable in sys.argv[1:]:
    result = subprocess.run([executable, '--version'], env=base_env,
                            capture_output=True, text=True, timeout=5)
    if result.returncode != 0:
        raise RuntimeError(f'{executable}: --version requires no fixture environment')
