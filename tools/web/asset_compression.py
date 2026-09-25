"""Add seekable, compressed browser assets without removing legacy raw files."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import struct
import tempfile
import zlib

BLOCK_SIZE = 64 * 1024
MIN_SIZE = 256 * 1024
MAGIC = b'DEZ1'
RAW_BLOCK = 0x80000000


def encode(source, target, size):
    """Write independent zlib blocks; keep incompressible blocks verbatim."""
    count = (size + BLOCK_SIZE - 1) // BLOCK_SIZE
    target.write(struct.pack('<4sIII', MAGIC, BLOCK_SIZE, size, count))
    target.write(bytes(count * 4))
    sizes = []
    original = hashlib.sha256()
    remaining = size
    while remaining:
        expected = min(BLOCK_SIZE, remaining)
        block = source.read(expected)
        if len(block) != expected:
            raise ValueError('Asset ended before its declared size')
        remaining -= len(block)
        original.update(block)
        compressed = zlib.compress(block, 3)
        if len(compressed) < len(block):
            target.write(compressed)
            sizes.append(len(compressed))
        else:
            target.write(block)
            sizes.append(len(block) | RAW_BLOCK)
    if source.read(1):
        raise ValueError('Asset exceeds its declared size')
    target.seek(16)
    target.write(struct.pack(f'<{count}I', *sizes))
    return original.hexdigest()


def compress_assets(root):
    root = Path(root).resolve()
    manifest_path = root / 'manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    if manifest.get('version') != 1 or not isinstance(manifest.get('files'), list):
        raise ValueError('Unsupported asset manifest')
    output = root / 'compressed'
    if output.is_symlink() or not output.resolve().is_relative_to(root):
        raise ValueError('Compressed output escapes its root')
    output.mkdir(exist_ok=True)
    total = retained = 0
    encoded = {}
    for entry in manifest['files']:
        name = entry['path']
        path = PurePosixPath(name)
        if (not name.startswith('Data/') or '\\' in name
                or any(part in ('', '.', '..') for part in name.split('/'))):
            raise ValueError(f'Invalid asset path: {name}')
        source = root.joinpath(*path.parts).resolve()
        if not source.is_relative_to(root):
            raise ValueError(f'Asset escapes its root: {name}')
        size = entry['size']
        if not isinstance(size, int) or size < 0 or size >= RAW_BLOCK:
            raise ValueError(f'Unsupported asset size: {name}')
        total += size
        if size < MIN_SIZE:
            entry.pop('compressed', None)
            retained += size
            continue
        # Content addressed and additive: old clients still read the raw file.
        digest = entry['sha256']
        if len(digest) != 64 or any(c not in '0123456789abcdef' for c in digest):
            raise ValueError(f'Invalid asset digest: {name}')
        # Rebuild from verified source bytes, even when a sidecar already exists.
        # Otherwise a damaged previous conversion could acquire a valid new hash.
        key = (digest, size)
        if key not in encoded:
            temporary = None
            try:
                with source.open('rb') as data, tempfile.NamedTemporaryFile(
                        dir=output, suffix='.tmp', delete=False) as packed:
                    temporary = Path(packed.name)
                    if encode(data, packed, size) != digest:
                        raise ValueError(f'Asset does not match its manifest: {name}')
                    packed_size = packed.seek(0, 2)
                    packed.seek(0)
                    checksum = hashlib.sha256()
                    while block := packed.read(1024 * 1024):
                        checksum.update(block)
                encoded[key] = None
                if packed_size < size:
                    compressed_digest = checksum.hexdigest()
                    filename = f'{compressed_digest}.dez'
                    # Name by the stored bytes: a different zlib version cannot
                    # replace bytes still referenced by an older manifest.
                    temporary.chmod(0o644)
                    temporary.replace(output / filename)
                    encoded[key] = {
                        'path': f'compressed/{filename}', 'size': packed_size,
                        'sha256': compressed_digest,
                    }
            finally:
                if temporary is not None:
                    temporary.unlink(missing_ok=True)
        compressed = encoded[key]
        if compressed is None:
            entry.pop('compressed', None)
            retained += size
            continue
        entry['compressed'] = compressed
        retained += compressed['size']
    # Replacing the manifest last makes interrupted conversion safe to retry.
    with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8', dir=root,
                                     suffix='.json.tmp', delete=False) as updated:
        json.dump(manifest, updated, indent=2)
        updated.write('\n')
        temporary = Path(updated.name)
    try:
        # NamedTemporaryFile starts at 0600 on Unix; nginx must be able to read it.
        temporary.chmod(0o644)
        temporary.replace(manifest_path)
    finally:
        temporary.unlink(missing_ok=True)
    print(f'Asset storage: {total:,} raw bytes -> {retained:,} stored bytes; '
          f'{total - retained:,} bytes saved before the bounded read cache')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path, help='existing packaged assets directory')
    compress_assets(parser.parse_args().directory)
