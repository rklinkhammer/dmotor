#!/usr/bin/env python3
"""Verify the first system-plan archive against retained bytes and Git source blobs."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
manifest = json.loads((root / 'docs/archive/pre-system-plan-2026-11-04/manifest.json').read_text())
for entry in manifest['entries']:
    original = (root / entry['original_copy']).read_bytes()
    page = (root / entry['archive_page']).read_bytes()
    if hashlib.sha256(original).hexdigest() != entry['original_sha256']:
        raise SystemExit('Original hash mismatch: ' + entry['original_path'])
    if hashlib.sha256(page).hexdigest() != entry['archive_sha256']:
        raise SystemExit('Browsable archive hash mismatch: ' + entry['archive_page'])
    source = subprocess.check_output(['git', '-C', str(root), 'cat-file', 'blob', entry['source_blob']])
    if original != source:
        raise SystemExit('Original differs from source Git blob: ' + entry['original_path'])
    for successor in entry['successors']:
        if not (root / successor).is_file():
            raise SystemExit('Missing successor: ' + successor)
print(f"Verified {len(manifest['entries'])} original copies, source blobs, archive hashes and successors")
