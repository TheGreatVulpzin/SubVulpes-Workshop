"""Tests for tools/build_index.py. Run: python -I tests/test_build_index.py"""
import hashlib
import json
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import build_index  # noqa: E402

MANIFEST = 'id={id}\nversion=0.1.0\nlibrary=lib.dll\napi=1\nlive=1\nlicense=GPL-3.0-only\n'


def make_png(size):
    def chunk(kind, data):
        body = kind + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body) & 0xffffffff)
    rows = b''.join(b'\x00' + b'\x80' * size for _ in range(size))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 0, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


def plugin(root, seal, plugin_id, extra='', icon=None, shots=0, manifest=None):
    folder = root / seal / 'plugins' / plugin_id
    folder.mkdir(parents=True)
    text = '[plugin]\n' + (manifest if manifest is not None else MANIFEST.format(id=plugin_id)) + extra
    (folder / 'plugin.ini').write_text(text, encoding='ascii')
    (folder / 'lib.dll').write_bytes(b'MZ' + plugin_id.encode() * 40)
    (folder / 'main.c').write_text('/* source */', encoding='ascii')
    if icon is not None:
        (folder / 'icon.png').write_bytes(icon)
    if shots:
        (folder / 'screenshots').mkdir()
        for n in range(shots):
            (folder / 'screenshots' / f'shot{n}.png').write_bytes(make_png(80 + n))
    return folder


class BuildIndexTests(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp(prefix='workshop-test-'))
        self.out = self.root / 'dist'
        self.addCleanup(shutil.rmtree, self.root, ignore_errors=True)
        for seal in ('official', 'community'):
            for kind in ('plugins', 'mods', 'bundles'):
                (self.root / seal / kind).mkdir(parents=True)
                (self.root / seal / kind / '.gitkeep').write_text('')

    def build(self, **kw):
        return build_index.build(self.root, self.out, generated='2026-01-01T00:00:00Z', **kw)

    def refused(self, needle):
        with self.assertRaises(build_index.Refused) as caught:
            self.build()
        self.assertIn(needle, str(caught.exception))

    def test_empty_tree(self):
        index = self.build()
        self.assertEqual(index, {'format': 1, 'generated': '2026-01-01T00:00:00Z', 'plugins': []})
        self.assertTrue((self.out / 'index.json').is_file())

    def test_entry_fields_and_seal(self):
        plugin(self.root, 'official', 'sv.one', 'name=One\nauthor=Vulpzin\ncapabilities=read,write\n'
               'dependencies=sv.lib@1.2,sv.other\ntags=admin,chat\nchannel=beta\nserver_min=0.1.1\n'
               'homepage=https://example.com\nicon=icon.png\n', icon=make_png(128), shots=2)
        plugin(self.root, 'community', 'ann.two')
        index = self.build()
        self.assertEqual([p['id'] for p in index['plugins']], ['ann.two', 'sv.one'])
        one = index['plugins'][1]
        self.assertEqual(one['source'], 'Official')
        self.assertEqual(index['plugins'][0]['source'], 'Community')
        self.assertEqual(one['author'], 'Vulpzin')
        self.assertEqual(one['capabilities'], ['read', 'write'])
        self.assertEqual(one['needs'], ['sv.lib', 'sv.other'])
        self.assertEqual(one['tags'], ['admin', 'chat'])
        self.assertEqual(one['channel'], 'beta')
        self.assertEqual(one['server_min'], '0.1.1')
        self.assertEqual(one['icon'], 'icons/sv.one.png')
        self.assertEqual(one['images'], ['images/sv.one-1.png', 'images/sv.one-2.png'])
        self.assertEqual(len(one['thumbs']), 2)
        for name in one['images'] + one['thumbs'] + [one['icon']]:
            self.assertTrue((self.out / name).is_file(), name)
        self.assertEqual(index['plugins'][0]['author'], '')

    def test_package_hash_size_and_content(self):
        plugin(self.root, 'official', 'sv.one', 'icon=icon.png\n', icon=make_png(64))
        entry = self.build()['plugins'][0]
        package = self.out / entry['package']['file']
        data = package.read_bytes()
        self.assertEqual(hashlib.sha256(data).hexdigest(), entry['package']['sha256'])
        self.assertEqual(len(data), entry['package']['size'])
        with zipfile.ZipFile(package) as archive:
            self.assertEqual(archive.namelist(), ['plugin.ini', 'lib.dll', 'icon.png'])

    def test_package_is_deterministic_and_leaves_source_out(self):
        plugin(self.root, 'official', 'sv.one')
        first = self.build()['plugins'][0]['package']
        second = self.build()['plugins'][0]['package']
        self.assertEqual(first, second)
        with zipfile.ZipFile(self.out / first['file']) as archive:
            self.assertNotIn('main.c', archive.namelist())

    def test_index_bytes_are_stable(self):
        plugin(self.root, 'official', 'sv.one')
        self.build()
        first = (self.out / 'index.json').read_bytes()
        self.build()
        self.assertEqual(first, (self.out / 'index.json').read_bytes())
        self.assertNotIn(b'\r', first)

    def test_refusals(self):
        plugin(self.root, 'community', 'a.nolicense', manifest='id=a.nolicense\nversion=1\nlibrary=lib.dll\napi=1\nlive=1\n')
        self.refused('no license')

    def test_folder_name_must_be_the_id(self):
        plugin(self.root, 'community', 'a.name', manifest=MANIFEST.format(id='a.other'))
        self.refused('folder name')

    def test_bad_ids(self):
        for bad in ('A.Upper', 'a..b'):
            shutil.rmtree(self.root / 'community' / 'plugins', ignore_errors=True)
            (self.root / 'community' / 'plugins').mkdir()
            plugin(self.root, 'community', bad)
            self.refused('id is not valid')

    def test_library_must_exist(self):
        folder = plugin(self.root, 'community', 'a.lib')
        (folder / 'lib.dll').unlink()
        self.refused('missing')

    def test_bad_icons(self):
        for icon in (b'text', make_png(32), make_png(1024), make_png(64) + b'\0' * 131100):
            shutil.rmtree(self.root / 'community' / 'plugins', ignore_errors=True)
            (self.root / 'community' / 'plugins').mkdir()
            plugin(self.root, 'community', 'a.icon', 'icon=icon.png\n', icon=icon)
            self.refused('icon')

    def test_missing_icon_file(self):
        plugin(self.root, 'community', 'a.icon', 'icon=icon.png\n')
        self.refused('icon file is missing')

    def test_reserved_values(self):
        for extra, needle in (('kind=mod\n', 'server plugins'), ('side=client\n', 'server plugins'), ('runtime=lua\n', 'native')):
            shutil.rmtree(self.root / 'community' / 'plugins', ignore_errors=True)
            (self.root / 'community' / 'plugins').mkdir()
            plugin(self.root, 'community', 'a.kind', extra)
            self.refused(needle)

    def test_mods_and_bundles_are_refused(self):
        for kind in ('mods', 'bundles'):
            (self.root / 'community' / kind / 'x').mkdir()
            self.refused('not accepted yet')
            (self.root / 'community' / kind / 'x').rmdir()

    def test_same_id_in_two_seals(self):
        plugin(self.root, 'official', 'sv.one')
        plugin(self.root, 'community', 'sv.one')
        self.refused('exists twice')

    def test_duplicate_key(self):
        plugin(self.root, 'community', 'a.dup', 'license=MIT\n')
        self.refused('twice')

    def test_too_many_or_bad_pictures(self):
        plugin(self.root, 'community', 'a.pics', shots=9)
        self.refused('at most 8')
        shutil.rmtree(self.root / 'community' / 'plugins' / 'a.pics')
        folder = plugin(self.root, 'community', 'a.pics', shots=1)
        (folder / 'screenshots' / 'bad name.png').write_bytes(b'x')
        self.refused('safe name')

    def test_stray_file_in_a_kind_folder(self):
        (self.root / 'community' / 'plugins' / 'stray.txt').write_text('x')
        self.refused('only folders')

    def test_notes_are_read_and_limited(self):
        folder = plugin(self.root, 'official', 'sv.one')
        (folder / 'changelog.txt').write_text('Line one.\r\nLine two.\n', encoding='ascii', newline='')
        (folder / 'breaking.txt').write_text('Old settings reset.', encoding='ascii')
        entry = self.build()['plugins'][0]
        self.assertEqual(entry['changelog'], 'Line one.\nLine two.')
        self.assertEqual(entry['breaking'], 'Old settings reset.')
        self.assertEqual(entry['versions'][0]['breaking'], 'Old settings reset.')
        (folder / 'breaking.txt').write_text('two\nlines', encoding='ascii')
        self.refused('one line only')
        (folder / 'breaking.txt').write_text('x' * 301, encoding='ascii')
        self.refused('at most 300')
        (folder / 'breaking.txt').unlink()
        (folder / 'changelog.txt').write_text('x' * 1201, encoding='ascii')
        self.refused('at most 1200')

    def test_old_versions_stay_in_the_index(self):
        folder = plugin(self.root, 'official', 'sv.one', 'capabilities=read\n')
        first = self.build()['plugins'][0]
        self.assertEqual([v['version'] for v in first['versions']], ['0.1.0'])
        previous = self.root / 'previous'
        shutil.copytree(self.out, previous)
        ini = (folder / 'plugin.ini').read_text(encoding='ascii').replace('version=0.1.0', 'version=0.2.0')
        (folder / 'plugin.ini').write_text(ini, encoding='ascii')
        second = self.build(previous=build_index.load_previous(previous))['plugins'][0]
        self.assertEqual([v['version'] for v in second['versions']], ['0.2.0', '0.1.0'])
        old = second['versions'][1]
        self.assertEqual(old['capabilities'], ['read'])
        self.assertEqual(hashlib.sha256((self.out / old['file']).read_bytes()).hexdigest(), old['sha256'])
        # A package whose hash is wrong is left out.
        (previous / old['file']).write_bytes(b'broken')
        third = self.build(previous=build_index.load_previous(previous))['plugins'][0]
        self.assertEqual([v['version'] for v in third['versions']], ['0.2.0'])

    def test_version_order(self):
        keys = ['0.2.9', '0.3.0-rc.1', '0.3.0', '0.10.0']
        keys.sort(key=build_index.version_key, reverse=True)
        self.assertEqual(keys, ['0.10.0', '0.3.0', '0.3.0-rc.1', '0.2.9'])

    def test_signature_with_openssl(self):
        """The documented signing step: an RSA key, a SHA-256 signature, and a check with the public key."""
        if not shutil.which('openssl'):
            self.skipTest('no openssl')
        plugin(self.root, 'official', 'sv.one')
        self.build()
        key, public, sig = self.root / 'k.pem', self.root / 'p.pem', self.root / 'index.json.sig'
        run = lambda *a: subprocess.run(a, capture_output=True, check=True)  # noqa: E731
        run('openssl', 'genrsa', '-out', str(key), '2048')
        run('openssl', 'rsa', '-in', str(key), '-pubout', '-out', str(public))
        run('openssl', 'dgst', '-sha256', '-sign', str(key), '-out', str(sig), str(self.out / 'index.json'))
        self.assertIn(b'Verified OK', run('openssl', 'dgst', '-sha256', '-verify', str(public), '-signature', str(sig),
                                          str(self.out / 'index.json')).stdout)
        (self.out / 'index.json').write_bytes((self.out / 'index.json').read_bytes() + b' ')
        bad = subprocess.run(['openssl', 'dgst', '-sha256', '-verify', str(public), '-signature', str(sig),
                              str(self.out / 'index.json')], capture_output=True)
        self.assertNotEqual(bad.returncode, 0)


if __name__ == '__main__':
    unittest.main(verbosity=1)
