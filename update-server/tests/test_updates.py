"""Tests of the update server (update_server.py, updates.py) with a fake
Amiga and a release made here (no GitHub).

  cd update-server && python -m unittest discover -s tests -v
"""
import asyncio
import io
import os
import shutil
import sys
import tempfile
import unittest
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import frames as P                    # noqa: E402
import updates as U                   # noqa: E402
import update_server as S             # noqa: E402


class Amiga:
    def __init__(self, port):
        self.port = port

    async def open(self):
        self.reader, self.writer = await asyncio.open_connection('127.0.0.1',
                                                                 self.port)
        return self

    def send(self, ftype, data=b''):
        self.writer.write(P.frame(ftype, data))

    async def recv(self, timeout=5):
        return await asyncio.wait_for(P.read_frame(self.reader), timeout)

    async def closed(self, timeout=5):
        try:
            while True:
                await asyncio.wait_for(P.read_frame(self.reader), timeout)
        except (asyncio.IncompleteReadError, ConnectionError, OSError):
            return True


class ServerTest(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.tmp = tempfile.mkdtemp()
        cfg = S.load_config()
        cfg.update({'port': 0, 'data_dir': self.tmp, 'listen': '127.0.0.1',
                    'check_s': 0})
        self.cfg = cfg
        self.server, self.listening = await S.start(cfg)
        self.port = self.listening.sockets[0].getsockname()[1]

    async def asyncTearDown(self):
        self.listening.close()
        await asyncio.sleep(0.05)
        shutil.rmtree(self.tmp, ignore_errors=True)

    async def amiga(self):
        return await Amiga(self.port).open()


HISTORY = """THE SETTLERS - history

0.140 (20.11.2026, beta release)
--------------------------------

- The fish grow slowly.

0.139 (18.11.2026, beta release)
--------------------------------

- Older.
"""


def make_zip():
    out = io.BytesIO()
    with zipfile.ZipFile(out, 'w') as z:
        z.writestr('Settlers/', '')
        z.writestr('Settlers/Settlers', b'\x00\x00\x03\xf3' + bytes(1000))
        z.writestr('Settlers/doc/Settlers_release_en.txt', HISTORY)
        z.writestr('Settlers/po/pl.po', 'msgid ""\n')
        z.writestr('Settlers.info', b'drawer icon')       # not in the drawer
        z.writestr('Settlers/../evil', b'no')              # never served
    return out.getvalue()


class Parts(unittest.TestCase):
    def test_fnv_as_the_game(self):
        # lan_data_checksum of the game: FNV-1a 32
        self.assertEqual(U.fnv1a(b''), 2166136261)
        self.assertEqual(U.fnv1a(b'a'), 0xe40c292c)

    def test_versions(self):
        self.assertGreater(U.version_key('0.140'), U.version_key('0.139'))
        self.assertGreater(U.version_key('1.2'), U.version_key('0.999'))
        self.assertIsNone(U.version_key('OpenDUNE-0.52'))

    def test_zip_and_notes(self):
        files = U.read_zip(make_zip(), 'Settlers/')
        self.assertEqual(sorted(files), ['Settlers', 'doc/Settlers_release_en.txt',
                                         'po/pl.po'])
        notes = U.notes_since(HISTORY, '0.140', '0.139')
        self.assertEqual(notes[0], '0.140 (20.11.2026, beta release)')
        self.assertIn('- The fish grow slowly.', notes)
        self.assertNotIn('- Older.', notes)
        self.assertEqual(U.notes_since(HISTORY, '0.140', '0.140'), [])


class Serve(ServerTest):
    async def asyncSetUp(self):
        await super().asyncSetUp()
        ups = self.server.updates
        self.release = ups.make_release('settlers', '0.140', make_zip())
        ups.releases['settlers'] = self.release

    async def ask(self, a, version='0.139'):
        a.send(P.F_RELAY, 'UPDATE 1 %s en' % version)
        lines = []
        while True:
            t, d = await a.recv()
            self.assertEqual(t, P.F_RELAY)
            lines.append(P.text(d))
            if lines[-1] in ('END',) or lines[-1].startswith('ERR'):
                return lines

    async def test_list_and_get(self):
        a = await self.amiga()
        lines = await self.ask(a)
        self.assertEqual(lines[0], 'LATEST 0.140')
        self.assertIn('NOTE - The fish grow slowly.', lines)
        files = [l.split() for l in lines if l.startswith('FILE ')]
        self.assertEqual([f[1] for f in files],
                         ['Settlers', 'doc/Settlers_release_en.txt', 'po/pl.po'])
        prog = self.release.files['Settlers']
        self.assertEqual(files[0][2:], [str(len(prog)), str(U.fnv1a(prog))])
        for path in ('Settlers', 'po/pl.po'):
            a.send(P.F_RELAY, 'GET ' + path)
            t, d = await a.recv()
            size = int(P.text(d).split()[1])
            got = b''
            while len(got) < size:
                t, d = await a.recv()
                self.assertEqual(t, P.F_RELAY_DATA)
                got += d
            self.assertEqual(got, self.release.files[path])
        a.send(P.F_RELAY, 'BYE')
        self.assertTrue(await a.closed())

    async def test_unknown_path_ends(self):
        a = await self.amiga()
        await self.ask(a)
        a.send(P.F_RELAY, 'GET ../evil')
        self.assertTrue(await a.closed())

    async def test_other_game(self):
        a = await self.amiga()
        lines = await self.ask(a, 'OpenDUNE-0.52')
        self.assertEqual(lines, ['ERR No release known.'])

    async def test_not_update(self):
        a = await self.amiga()
        a.send(P.F_RELAY, 'LIST 1 0.139')
        t, d = await a.recv()
        self.assertEqual(P.text(d), 'ERR Unknown command.')

    async def test_kept_on_disk(self):
        ups = self.server.updates
        ups.keep('settlers', self.release)
        again = U.Updates(self.cfg)
        self.assertEqual(again.releases['settlers'].version, '0.140')
        self.assertEqual(again.releases['settlers'].manifest,
                         self.release.manifest)


if __name__ == '__main__':
    unittest.main()
