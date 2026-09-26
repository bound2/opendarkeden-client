import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import random
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile
import zlib

spec = importlib.util.spec_from_file_location(
    'compress_assets', Path(__file__).parents[2] / 'tools/web/asset_compression.py')
codec = importlib.util.module_from_spec(spec)
spec.loader.exec_module(codec)


def decode(data):
    magic, block_size, size, count = struct.unpack_from('<4sIII', data)
    assert magic == b'DEZ1' and block_size == 65536
    offset = 16 + count * 4
    output = bytearray()
    for stored in struct.unpack_from(f'<{count}I', data, 16):
        length = stored & ~codec.RAW_BLOCK
        block = data[offset:offset + length]
        output.extend(block if stored & codec.RAW_BLOCK else zlib.decompress(block))
        offset += length
    assert len(output) == size and offset == len(data)
    return bytes(output)


class CompressedAssets(unittest.TestCase):
    def test_random_compressible_empty_and_partial_blocks_round_trip(self):
        for source in [b'', b'x', b'ABCD' * 50000, random.Random(5).randbytes(170003)]:
            with self.subTest(length=len(source)):
                packed = io.BytesIO()
                self.assertEqual(hashlib.sha256(source).hexdigest(),
                                 codec.encode(io.BytesIO(source), packed, len(source)))
                self.assertEqual(source, decode(packed.getvalue()))

    def test_declared_size_must_match(self):
        for size in [2, 4]:
            with self.assertRaises(ValueError):
                codec.encode(io.BytesIO(b'abc'), io.BytesIO(), size)

    def test_conversion_preserves_raw_files_and_is_repeatable(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'Data').mkdir()
            original = b'ABCD' * 100000
            (root / 'Data/file.spk').write_bytes(original)
            entry = {'path': 'Data/file.spk', 'size': len(original),
                     'sha256': hashlib.sha256(original).hexdigest()}
            manifest = root / 'manifest.json'
            manifest.write_text(json.dumps({'version': 1, 'files': [entry]}))
            codec.compress_assets(root)
            first = manifest.read_bytes()
            packed_path = root / json.loads(first)['files'][0]['compressed']['path']
            packed_path.write_bytes(b'damaged previous conversion')
            codec.compress_assets(root)
            self.assertEqual(first, manifest.read_bytes())
            self.assertEqual(original, (root / 'Data/file.spk').read_bytes())
            compressed = json.loads(first)['files'][0]['compressed']
            packed = (root / compressed['path']).read_bytes()
            self.assertEqual(original, decode(packed))
            self.assertEqual(hashlib.sha256(packed).hexdigest(), compressed['sha256'])
            self.assertEqual(f"compressed/{compressed['sha256']}.dez", compressed['path'])
            if os.name != 'nt':
                self.assertEqual(0o644, packed_path.stat().st_mode & 0o777)
                self.assertEqual(0o644, manifest.stat().st_mode & 0o777)

    def test_bad_source_hash_does_not_publish_a_new_manifest(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'Data').mkdir()
            (root / 'Data/file.spk').write_bytes(b'x' * codec.MIN_SIZE)
            manifest = root / 'manifest.json'
            before = json.dumps({'version': 1, 'files': [{
                'path': 'Data/file.spk', 'size': codec.MIN_SIZE, 'sha256': '0' * 64}]})
            manifest.write_text(before)
            with self.assertRaises(ValueError):
                codec.compress_assets(root)
            self.assertEqual(before, manifest.read_text())
            self.assertEqual([], list((root / 'compressed').iterdir()))

    def test_small_and_incompressible_files_keep_raw_storage(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'Data').mkdir()
            files = []
            for name, source in [('small', b'small'),
                                 ('random', random.Random(7).randbytes(codec.MIN_SIZE))]:
                (root / 'Data' / name).write_bytes(source)
                files.append({'path': f'Data/{name}', 'size': len(source),
                              'sha256': hashlib.sha256(source).hexdigest(),
                              'compressed': {'path': 'stale metadata'}})
            manifest = root / 'manifest.json'
            manifest.write_text(json.dumps({'version': 1, 'files': files}))
            codec.compress_assets(root)
            self.assertTrue(all('compressed' not in entry
                                for entry in json.loads(manifest.read_text())['files']))
            self.assertEqual([], list((root / 'compressed').iterdir()))

    def test_packer_produces_compressed_sidecars_by_default(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / 'assets.zip'
            original = b'sprite' * codec.MIN_SIZE
            with zipfile.ZipFile(archive, 'w') as data:
                data.writestr('Data/Info/FileDef.inf', b'fixture')
                data.writestr('Data/fixture.spk', original)
                data.writestr('UserSet/private.set', b'never package')
            font = root / 'font.ttc'
            font.write_bytes(b'font fixture')
            (root / 'LICENSE.txt').write_text('fixture license')
            output = root / 'output'
            subprocess.run([sys.executable, str(Path(__file__).parents[2] /
                'tools/web/package-assets.py'), str(archive), str(output), '--font', str(font)],
                check=True, capture_output=True)
            files = json.loads((output / 'manifest.json').read_text())['files']
            self.assertTrue(all(entry['path'].startswith('Data/') for entry in files))
            entry = next(entry for entry in files if entry['path'] == 'Data/fixture.spk')
            self.assertEqual(original, decode((output / entry['compressed']['path']).read_bytes()))
            self.assertEqual(original, (output / entry['path']).read_bytes())


if __name__ == '__main__':
    unittest.main()
