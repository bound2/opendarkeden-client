"""Build assets-v2 from the immutable assets-v1 release, preserving other files."""
import argparse
import copy
import hashlib
import json
import re
import shutil
import struct
import zipfile
from pathlib import Path

V1_SHA256 = '9401fd9c294123173222744fc4298ba503fb92ca1004356d983a228db3447985'
ORIGINAL_SHA256 = 'c7e277c1104a13db305619e2c5b1d4b69fa27d6e7e6919ab2239713ac6042e21'
MISSING_V1_FILES = (
    'Data/Info/NPC.inf', 'Data/Info/NPCScript.inf',
    'Data/Ui/spk/mixingforge.spk', 'Data/Ui/spk/mixingforge.spki',
    'Data/Ui/spk/monsterlevel.spk', 'Data/Ui/spk/monsterlevel.spki',
    'Data/Ui/spk/trace.spk', 'Data/Ui/spk/trace.spki',
)
BRAND = re.compile(rb'dk2th', re.I)
TOKEN = re.compile(rb'(?<![A-Za-z0-9\x80-\xff])(?:https?://)?(?:www\.)?dk2th(?:\.com)?(?![A-Za-z0-9\x80-\xff])', re.I)


def clean_name(value):
    if not BRAND.search(value):
        return value
    value = re.sub(rb'[ \t_]*' + TOKEN.pattern + rb'[ \t_]*', b' ', value, flags=re.I)
    return value.strip(b' \t\r\n')


def clean_message(value):
    # These are English strings in the original mixed-language String.inf.
    # Keep format conversions and all non-branding prose byte-for-byte.
    if TOKEN.fullmatch(value.strip()):
        return b''
    value = value.replace(b' (www.dk2th.com)', b'')
    value = value.replace(b'Please verify at www.dk2th.com', b'Please contact customer support.')
    value = value.replace(b'please visit www.dk2th.com.', b'please contact customer support.')
    value = value.replace(b'please visit www.dk2th.com', b'please contact customer support.')
    value = re.sub(rb'Please visit www\.dk2th\.com', b'Please contact customer support.', value, flags=re.I)
    value = value.replace(b'[DK2TH]', b'')
    value = re.sub(rb'\bDK2TH\b', b'DarkEden', value, flags=re.I)
    return value


class Table:
    def __init__(self, data, name):
        self.data, self.name, self.pos = data, name, 0
        self.parts, self.changes = [], []

    def raw(self, size):
        if size < 0 or self.pos + size > len(self.data):
            raise ValueError(f'{self.name}: invalid read at {self.pos} of {size} bytes')
        value = self.data[self.pos:self.pos + size]
        self.pos += size
        self.parts.append(value)
        return value

    def number(self, size=4):
        return int.from_bytes(self.raw(size), 'little')

    def string(self, field, cleaner=None):
        offset = self.pos
        size = self.number()
        value = self.raw(size)
        new = cleaner(value) if cleaner else value
        if cleaner and BRAND.search(new):
            raise ValueError(f'{self.name}/{field}: branding remains in display text')
        if new != value:
            self.parts[-2:] = [struct.pack('<I', len(new)), new]
            self.changes.append({'field': field, 'offset': offset,
                'before': value.decode('ascii', 'backslashreplace'),
                'after': new.decode('ascii', 'backslashreplace')})
        return value

    def finish(self):
        # The v1 item and creature tables carry unused all-zero tails.
        tail = self.data[self.pos:]
        if tail:
            if any(tail):
                raise ValueError(f'{self.name}: nonzero trailing data at {self.pos}')
            self.raw(len(tail))
        return b''.join(self.parts)


def transform(data, name, clean=True):
    t = Table(data, name)
    names = clean_name if clean else None
    if name == 'Item.inf':
        for group in range(t.number()):
            for item in range(t.number()):
                for field in ('EName', 'HName', 'Description'):
                    t.string(f'{group}/{item}/{field}', names)
                t.raw(85)  # Fixed fields before the default-option list.
                t.raw(t.number(1))  # TYPE_ITEM_OPTION is one byte.
                t.raw(13)
    elif name == 'Zone.inf':
        for row in range(t.number()):
            zone = t.number(2)
            t.string(f'{zone}/Name', names)
            t.raw(5)
            for field in ('Filename', 'InfoFilename', 'TeenFilename'):
                t.string(f'{zone}/{field}')
            t.raw(t.number() * 2)
            t.raw(11)
    elif name == 'Creature.inf':
        for row in range(t.number()):
            # Some placeholder creatures were named solely after the old
            # server. Keep a usable name rather than turning them invisible.
            cleaner = (lambda v: (clean_name(v) or b'Creature') if BRAND.search(v) else v) if clean else None
            t.string(f'{row}/Name', cleaner)
            sprites = [t.number() for _ in range(t.number())]
            t.raw(1)
            tribe = t.number(1)
            actions = {0: 35, 1: 18 if sprites and sprites[0] == 204 else 11,
                       2: 11, 3: 35, 4: 18, 5: 18}[tribe]
            t.raw(34 + actions * 6)
            if t.number(1) and tribe not in (4, 5):
                t.raw(23)
    elif name == 'String.inf':
        for row in range(t.number()):
            t.string(str(row), clean_message if clean else None)
    else:
        raise ValueError(name)
    result = t.finish()
    # Four v1 zones have the bytes 'dk2th' in their five-byte numeric
    # property/music fields. They are not text; preserve their exact values.
    if clean and name != 'Zone.inf' and BRAND.search(result):
        raise ValueError(f'{name}: branding remains after parsing all records')
    return result, t.changes


def sha256(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--original', type=Path, required=True,
                   help='Original DARKEDEN.zip containing hidden files omitted from v1')
    p.add_argument('--audit-only', action='store_true')
    args = p.parse_args()
    if sha256(args.source) != V1_SHA256:
        raise ValueError('Input is not the published assets-v1 archive')
    if sha256(args.original) != ORIGINAL_SHA256:
        raise ValueError('Original distribution checksum does not match')
    replacements, report = {}, {}
    additions = {}
    with zipfile.ZipFile(args.original) as original:
        for name in MISSING_V1_FILES:
            data = original.read(name)
            if BRAND.search(data):
                raise ValueError(f'Branding in restored file: {name}')
            additions[name] = (copy.copy(original.getinfo(name)), data)
            report[name] = {'restored_from': 'DARKEDEN.zip',
                            'sha256': hashlib.sha256(data).hexdigest()}
    with zipfile.ZipFile(args.source) as src:
        if set(additions).intersection(src.namelist()):
            raise ValueError('A restored file already exists in v1')
        for info in src.infolist():
            if info.filename in ('Data/Info/Item.inf', 'Data/Info/Zone.inf',
                                  'Data/Info/Creature.inf', 'Data/Info/String.inf'):
                old = src.read(info)
                new, changes = transform(old, Path(info.filename).name)
                # Reparse the rewritten data and prove it needs no more edits.
                assert transform(new, Path(info.filename).name)[0] == new
                replacements[info.filename] = new
                report[info.filename] = {'before_sha256': hashlib.sha256(old).hexdigest(),
                    'after_sha256': hashlib.sha256(new).hexdigest(), 'changes': changes}
                print(info.filename, len(changes), 'text fields changed', flush=True)
        if len(replacements) != 4:
            raise ValueError('Archive is missing one of the expected text tables')
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.with_suffix('.changes.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
        if args.audit_only:
            return
        if args.output.exists():
            raise FileExistsError(args.output)
        with zipfile.ZipFile(args.output, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as dst:
            for info in src.infolist():
                if info.filename in replacements:
                    dst.writestr(copy.copy(info), replacements[info.filename])
                else:
                    with src.open(info) as incoming, dst.open(copy.copy(info), 'w') as outgoing:
                        shutil.copyfileobj(incoming, outgoing, 1024 * 1024)
            for info, data in additions.values():
                dst.writestr(copy.copy(info), data)
        print('Verifying archive contents and CRCs...', flush=True)
        with zipfile.ZipFile(args.output) as dst:
            assert dst.namelist() == src.namelist() + list(additions)
            for name, (_, data) in additions.items():
                assert dst.read(name) == data
            for info in src.infolist():
                data = dst.read(info.filename)
                if info.filename in replacements:
                    assert data == replacements[info.filename]
                else:
                    assert hashlib.sha256(data).digest() == hashlib.sha256(src.read(info)).digest()
                if info.filename == 'Data/Info/Zone.inf':
                    assert len(BRAND.findall(data)) == 4  # Preserved numeric fields.
                elif BRAND.search(data):
                    raise ValueError(f'Branding bytes remain in {info.filename}')
        digest = sha256(args.output)
        args.output.with_suffix('.zip.sha256').write_text(f'{digest}  {args.output.name}\n', encoding='ascii')
        print(digest, args.output, flush=True)


if __name__ == '__main__':
    main()
