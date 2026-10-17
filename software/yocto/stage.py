#!/usr/bin/env python3
"""Stage deterministic sources and provenance; EDF normally calls this automatically."""
import argparse
import gzip
import io
import json
from pathlib import Path
import sys
import tarfile
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('edf', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'software'))
from version import provenance, write
info = provenance(root)
layer = args.edf.resolve() / 'meta-edf-dev'
if not (layer / 'conf/layer.conf').is_file():
    parser.error('Not an EDF checkout')
dest = layer / 'recipes-dmotor/dmotor-semantic-tests'
(dest / 'files').mkdir(parents=True, exist_ok=True)
raw = io.BytesIO()
with tarfile.open(fileobj=raw, mode='w') as archive:
    sources = {name: (root/name).read_bytes() for name in info['files']}
    sources['software/source-version.json'] = (json.dumps(info, indent=2, sort_keys=True)+'\n').encode()
    for name, content in sorted(sources.items()):
        entry = tarfile.TarInfo('dmotor-software/'+name)
        entry.size = len(content); entry.mode = 0o644
        archive.addfile(entry, io.BytesIO(content))
archive_path = dest/'files/dmotor-software.tar.gz'
data = gzip.compress(raw.getvalue(), mtime=0)
if not archive_path.exists() or archive_path.read_bytes() != data:
    archive_path.write_bytes(data)
write(dest/'dmotor-semantic-tests.bb', (root/'software/yocto/dmotor-semantic-tests.bb').read_text())
print('DMOTOR staged: '+info['git_commit']+' source='+info['source_sha256'])

driver_dest = layer / "recipes-dmotor/dmotor-access"
driver_dest.mkdir(parents=True, exist_ok=True)
write(driver_dest / "dmotor-access.bb", (root / "software/yocto/dmotor-access.bb").read_text())
