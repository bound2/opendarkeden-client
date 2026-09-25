"""Prepare a separate, verified browser data pack from a user's asset ZIP."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import zipfile


def package(archive, output, font):
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    files = []
    seen = set()
    folded = set()

    def copy(source, name):
        path = PurePosixPath(name)
        if (path.is_absolute() or '..' in path.parts or '\\' in name
                or not name.startswith('Data/') or name.lower() in folded):
            raise ValueError(f'Invalid or repeated asset path: {name}')
        seen.add(name)
        folded.add(name.lower())
        destination = output.joinpath(*path.parts).resolve()
        if not destination.is_relative_to(output):
            raise ValueError(f'Asset escapes output directory: {name}')
        destination.parent.mkdir(parents=True, exist_ok=True)
        digest = hashlib.sha256()
        size = 0
        with destination.open('wb') as target:
            while chunk := source.read(1024 * 1024):
                target.write(chunk)
                digest.update(chunk)
                size += len(chunk)
        files.append({'path': name, 'size': size, 'sha256': digest.hexdigest()})

    with zipfile.ZipFile(archive) as data:
        for entry in data.infolist():
            # UserSet may contain private account settings; never distribute it.
            if entry.is_dir() or not entry.filename.startswith('Data/'):
                continue
            if (entry.external_attr >> 16) & 0o170000 == 0o120000:
                raise ValueError(f'Symbolic link in asset archive: {entry.filename}')
            with data.open(entry) as source:
                copy(source, entry.filename)
    if 'Data/Info/FileDef.inf' not in seen:
        raise ValueError('Archive must contain Data/Info/FileDef.inf at its root')
    # Native clients use system fonts; a browser deployment needs its own font.
    if font:
        with font.open('rb') as source:
            copy(source, 'Data/Font/NotoSansCJK-Regular.ttc')
        license = font.parent / 'LICENSE.txt'
        if not license.is_file():
            raise ValueError('Place the font license beside the font as LICENSE.txt')
        with license.open('rb') as source:
            copy(source, 'Data/Font/LICENSE.txt')
    if 'Data/Font/NotoSansCJK-Regular.ttc' not in seen:
        raise ValueError('Supply a Noto Sans CJK TTC with --font')
    files.sort(key=lambda entry: entry['path'])
    (output / 'manifest.json').write_text(json.dumps({'version': 1, 'files': files}, indent=2) + '\n', encoding='utf-8')
    print(f'Packaged {len(files)} files, {sum(entry["size"] for entry in files):,} bytes into {output}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--font', type=Path)
    args = parser.parse_args()
    package(args.archive, args.output, args.font)
