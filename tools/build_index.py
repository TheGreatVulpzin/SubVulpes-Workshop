#!/usr/bin/env python3
"""Builds the Workshop packages and the catalog index.

It reads each plugin folder, checks it, packs it into a .svplugin file and writes dist/index.json.
Use: python -I tools/build_index.py [--root .] [--out dist] [--server-exe subvulpes_server.exe]

The server program holds the full manifest rules. With --server-exe the script asks it to check each folder.
This script checks the rules that the catalog needs: the id, the license, the library, the icon and the pictures.
"""
import argparse
import hashlib
import json
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile
from datetime import datetime, timezone
from pathlib import Path

SEALS = {'official': 'Official', 'community': 'Community'}
KINDS = ('plugins', 'mods', 'bundles')
ID_RULE = re.compile(r'^[a-z0-9][a-z0-9._-]{0,47}$')
ICON_NAME = re.compile(r'^[a-z0-9][a-z0-9._-]{0,43}\.png$')
PICTURE = re.compile(r'^[A-Za-z0-9][A-Za-z0-9._-]{0,63}\.(png|jpg)$')
PICTURE_LIMIT = 2 * 1024 * 1024
MAX_PICTURES = 8
THUMB_WIDTH = 320
STAMP = (2000, 1, 1, 0, 0, 0)
CHANGELOG_MAX = 1200
BREAKING_MAX = 300
VERSIONS_MAX = 24


class Refused(Exception):
    pass


def read_manifest(path):
    """The values of the section [plugin]. A key that appears twice is refused."""
    values, inside = {}, False
    for raw in path.read_text(encoding='ascii', errors='strict').splitlines():
        line = raw.strip()
        if not line or line.startswith(('#', ';')):
            continue
        if line.startswith('['):
            inside = line == '[plugin]'
            continue
        if inside and '=' in line:
            key, value = line.split('=', 1)
            if key in values:
                raise Refused(f'{path}: the key {key} appears twice.')
            values[key] = value.strip()
    return values


def png_size(data):
    """The size of a PNG from its header, or None when the data is not a PNG."""
    if len(data) < 24 or data[:16] != b'\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR':
        return None
    return struct.unpack('>II', data[16:24])


def check_icon(data, where):
    size = png_size(data)
    if len(data) > 131072 or size is None or size[0] != size[1] or not 64 <= size[0] <= 512:
        raise Refused(f'{where}: the icon must be a square PNG from 64 to 512 pixels and at most 128 KiB.')


def read_note(folder, name, limit, one_line):
    """The text of changelog.txt or breaking.txt in a plugin folder. A text over the limit is refused, not cut."""
    path = folder / name
    if not path.is_file():
        return ''
    text = path.read_text(encoding='utf-8').replace('\r\n', '\n').strip()
    if len(text) > limit:
        raise Refused(f'{path}: at most {limit} characters.')
    if one_line and '\n' in text:
        raise Refused(f'{path}: one line only.')
    return text


def version_key(version):
    """Sort key for a plugin version such as 0.3.0 or 0.3.0-rc.1. A candidate comes before its release."""
    core, _, suffix = version.partition('-')
    numbers = tuple(int(n) if n.isdigit() else 0 for n in core.split('.'))
    return (numbers, 1 if not suffix else 0, suffix)


def version_item(entry):
    """The item of the list `versions` that describes the top level of an entry."""
    item = {'version': entry['version'], 'file': entry['package']['file'], 'sha256': entry['package']['sha256'],
            'size': entry['package']['size'], 'min_api': entry['min_api'], 'channel': entry['channel']}
    for key in ('server_min', 'changelog', 'breaking', 'capabilities', 'runtime'):
        if entry.get(key):
            item[key] = entry[key]
    return item


def older_versions(entry, previous, out):
    """The versions of the last published index, with their packages. A package whose hash is wrong is left out."""
    before = next((p for p in previous.get('plugins', []) if p.get('id') == entry['id']), None)
    if before is None:
        return []
    items = before.get('versions') or [version_item(before)]
    kept = []
    for item in items:
        if item.get('version') == entry['version'] or not str(item.get('file', '')).startswith('packages/'):
            continue
        source = previous['_root'] / item['file']
        if not source.is_file() or hashlib.sha256(source.read_bytes()).hexdigest() != item.get('sha256'):
            print(f'WARNING: the old package {item.get("file")} is missing or wrong. It is left out.', file=sys.stderr)
            continue
        target = out / item['file']
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        kept.append(item)
    return kept


def needs_of(text):
    return [item.split('@', 1)[0] for item in text.split(',') if item]


def pack(folder, values, target):
    """A package with a fixed order and a fixed time stamp, so the same folder gives the same bytes."""
    names = ['plugin.ini', values['library']]
    if 'icon' in values:
        names.append(values['icon'])
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name in names:
            entry = zipfile.ZipInfo(name, STAMP)
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o644 << 16
            archive.writestr(entry, (folder / name).read_bytes())


def pictures(folder):
    shots = folder / 'screenshots'
    if not shots.is_dir():
        return []
    found = sorted(p for p in shots.iterdir() if p.is_file() and p.name != '.gitkeep')
    for p in found:
        if not PICTURE.match(p.name) or p.stat().st_size > PICTURE_LIMIT:
            raise Refused(f'{p}: a picture is a PNG or JPEG with a safe name and at most 2 MiB.')
    if len(found) > MAX_PICTURES:
        raise Refused(f'{shots}: at most {MAX_PICTURES} pictures.')
    return found


def make_thumb(source, target):
    """A small JPEG for the list. Without Pillow the thumbnail is the picture itself."""
    try:
        from PIL import Image
    except ImportError:
        shutil.copyfile(source, target)
        return target
    with Image.open(source) as image:
        image = image.convert('RGB')
        height = max(1, round(image.height * THUMB_WIDTH / image.width))
        image.resize((THUMB_WIDTH, height)).save(target, 'JPEG', quality=82)
    return target


def check_folder(folder, server_exe):
    manifest_path = folder / 'plugin.ini'
    if not manifest_path.is_file():
        raise Refused(f'{folder}: no plugin.ini.')
    values = read_manifest(manifest_path)
    for key in ('id', 'version', 'library', 'license'):
        if not values.get(key):
            raise Refused(f'{folder}: the manifest has no {key}.')
    if not ID_RULE.match(values['id']) or '..' in values['id']:
        raise Refused(f'{folder}: the id is not valid.')
    if values['id'] != folder.name:
        raise Refused(f'{folder}: the folder name must be the id.')
    if values.get('kind', 'plugin') != 'plugin' or values.get('side', 'server') != 'server':
        raise Refused(f'{folder}: only server plugins are accepted now.')
    if values.get('runtime', 'native') != 'native':
        raise Refused(f'{folder}: only native plugins are accepted now.')
    library = folder / values['library']
    if not library.is_file() or '/' in values['library'] or '\\' in values['library']:
        raise Refused(f'{folder}: the library {values["library"]} is missing. Build it first.')
    if 'icon' in values:
        if not ICON_NAME.match(values['icon']) or '..' in values['icon']:
            raise Refused(f'{folder}: the icon name is not valid.')
        icon = folder / values['icon']
        if not icon.is_file():
            raise Refused(f'{folder}: the icon file is missing.')
        check_icon(icon.read_bytes(), folder)
    return values


def stage_of(folder, values):
    """A temporary folder with the files of the package only. The source folder holds more."""
    stage = Path(tempfile.mkdtemp(prefix='workshop-'))
    for name in ['plugin.ini', values['library']] + ([values['icon']] if 'icon' in values else []):
        shutil.copyfile(folder / name, stage / name)
    return stage


def build_entry(seal, folder, values, out, server_exe):
    stage = stage_of(folder, values)
    try:
        # The server checks the staged files, which are the files that the package carries.
        if server_exe:
            result = subprocess.run([str(server_exe), '--plugin-validate-folder', str(stage)],
                                    capture_output=True, text=True, timeout=120)
            if result.returncode != 0:
                raise Refused(f'{folder}: the server refused the manifest:\n{result.stdout}')
        runtime = values.get('runtime', 'native')
        package_name = f'{values["id"]}-{values["version"]}-{runtime}.svplugin'
        package = out / 'packages' / package_name
        package.parent.mkdir(parents=True, exist_ok=True)
        pack(stage, values, package)
    finally:
        shutil.rmtree(stage, ignore_errors=True)
    data = package.read_bytes()
    entry = {
        'id': values['id'],
        'name': values.get('name', values['id']),
        'description': values.get('description', ''),
        'author': values.get('author', ''),
        'version': values['version'],
        'kind': 'plugin',
        'source': SEALS[seal],
        'license': values['license'],
        'runtime': values.get('runtime', 'native'),
        'capabilities': [c for c in values.get('capabilities', '').split(',') if c],
        'min_api': int(values.get('api', '1')),
        'needs': needs_of(values.get('dependencies', '')),
        'tags': [t for t in values.get('tags', '').split(',') if t],
        'channel': values.get('channel', 'stable'),
        'package': {'file': f'packages/{package_name}', 'sha256': hashlib.sha256(data).hexdigest(), 'size': len(data)},
        'thumbs': [],
        'images': [],
    }
    for key, name, limit, one_line in (('changelog', 'changelog.txt', CHANGELOG_MAX, False), ('breaking', 'breaking.txt', BREAKING_MAX, True)):
        note = read_note(folder, name, limit, one_line)
        if note:
            entry[key] = note
    if values.get('homepage'):
        entry['homepage'] = values['homepage']
    if values.get('server_min'):
        entry['server_min'] = values['server_min']
    if 'icon' in values:
        (out / 'icons').mkdir(parents=True, exist_ok=True)
        shutil.copyfile(folder / values['icon'], out / 'icons' / f'{values["id"]}.png')
        entry['icon'] = f'icons/{values["id"]}.png'
    for number, picture in enumerate(pictures(folder), 1):
        extension = picture.suffix.lower()
        (out / 'images').mkdir(parents=True, exist_ok=True)
        (out / 'thumbs').mkdir(parents=True, exist_ok=True)
        image_name = f'{values["id"]}-{number}{extension}'
        thumb_name = f'{values["id"]}-{number}.jpg' if extension == '.jpg' else f'{values["id"]}-{number}.png'
        shutil.copyfile(picture, out / 'images' / image_name)
        make_thumb(picture, out / 'thumbs' / thumb_name)
        entry['images'].append(f'images/{image_name}')
        entry['thumbs'].append(f'thumbs/{thumb_name}')
    return entry


def content_folders(root):
    """The plugin folders as (seal, folder). A folder of mods or bundles with content is refused for now."""
    found = []
    for seal in SEALS:
        for kind in KINDS:
            base = root / seal / kind
            if not base.is_dir():
                continue
            children = [c for c in sorted(base.iterdir()) if c.name != '.gitkeep']
            if kind != 'plugins' and children:
                raise Refused(f'{base}: mods and bundles are not accepted yet.')
            for child in children:
                if not child.is_dir():
                    raise Refused(f'{child}: only folders belong here.')
                found.append((seal, child))
    return found


def build(root, out, server_exe=None, generated=None, previous=None):
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    entries, seen = [], set()
    for seal, folder in content_folders(root):
        values = check_folder(folder, server_exe)
        if values['id'] in seen:
            raise Refused(f'{folder}: the id {values["id"]} exists twice.')
        seen.add(values['id'])
        entries.append(build_entry(seal, folder, values, out, server_exe))
    entries.sort(key=lambda e: e['id'])
    # The list of versions: this build first, then the versions of the last published index, newest first.
    for entry in entries:
        items = [version_item(entry)] + (older_versions(entry, previous, out) if previous else [])
        items.sort(key=lambda item: version_key(item['version']), reverse=True)
        entry['versions'] = items[:VERSIONS_MAX]
    stamp = generated or datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
    index = {'format': 1, 'generated': stamp, 'plugins': entries}
    # A fixed layout keeps the bytes the same for the same content, so the signature stays checkable.
    (out / 'index.json').write_text(json.dumps(index, indent=2, ensure_ascii=True) + '\n', encoding='ascii', newline='\n')
    return index


def load_previous(folder):
    """The last published index, or nothing when there is none yet."""
    path = folder / 'index.json'
    if not path.is_file():
        return None
    data = json.loads(path.read_text(encoding='ascii'))
    data['_root'] = folder
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--root', default='.')
    parser.add_argument('--out', default='dist')
    parser.add_argument('--server-exe')
    parser.add_argument('--generated')
    parser.add_argument('--previous', help='A folder with the last published index.json and packages.')
    args = parser.parse_args()
    try:
        index = build(Path(args.root).resolve(), Path(args.out).resolve(),
                      Path(args.server_exe).resolve() if args.server_exe else None, args.generated,
                      load_previous(Path(args.previous).resolve()) if args.previous else None)
    except Refused as error:
        print(f'REFUSED: {error}', file=sys.stderr)
        return 1
    print(f'{len(index["plugins"])} plugins in the index.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
