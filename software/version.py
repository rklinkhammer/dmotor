#!/usr/bin/env python3
"""Generate deterministic source provenance; preserve staged Git identity."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

def provenance(root):
    names = json.loads((root / 'software/sources.json').read_text())
    hashes = {name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in sorted(names)}
    fingerprint = hashlib.sha256(json.dumps(hashes, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    staged = root / 'software/source-version.json'
    if staged.exists():
        info = json.loads(staged.read_text())
        if info['source_sha256'] != fingerprint:
            raise RuntimeError('Staged source differs from source-version.json')
        return info
    def git(*args):
        return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()
    return dict(version=(root / 'software/VERSION').read_text().strip(),
                git_commit=git('rev-parse', 'HEAD'), dirty=bool(git('status', '--porcelain', '--untracked-files=normal')),
                source_sha256=fingerprint, controller_abi='DMI1/DMB1-schema1', files=hashes)

def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text() != text:
        path.write_text(text)

if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    info = provenance(root)
    output = Path(sys.argv[1])
    write(output / 'source-version.json', json.dumps(info, indent=2, sort_keys=True)+'\n')
    banner = f"DMOTOR {info['version']} commit={info['git_commit']} dirty={str(info['dirty']).lower()} source={info['source_sha256']} ABI={info['controller_abi']}"
    write(output / 'dmotor_version_generated.hpp', '#pragma once\n#define DMOTOR_VERSION_BANNER '+json.dumps(banner)+'\n')
