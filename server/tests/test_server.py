"""Tests of the server with fake Amigas (the frames of port/amiga_net.c).

  cd server && python -m unittest discover -s tests -v
  LOAD=1 python -m unittest tests.test_server.Load   (50 games, ~1 minute)
"""
import asyncio
import json
import os
import shutil
import struct
import sys
import tempfile
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import protocol as P                  # noqa: E402
import settlers_server as S           # noqa: E402

VERSION = '0.131'


class Amiga:
    """A fake Amiga: the frames of the game, over TCP."""

    def __init__(self, port):
        self.port = port

    async def open(self):
        self.reader, self.writer = await asyncio.open_connection('127.0.0.1', self.port)
        return self

    def send(self, ftype, data=b''):
        self.writer.write(P.frame(ftype, data))

    async def recv(self, timeout=5):
        return await asyncio.wait_for(P.read_frame(self.reader), timeout)

    async def relay(self, line):
        self.send(P.F_RELAY, line)
        t, d = await self.recv()
        assert t == P.F_RELAY, (t, d)
        return P.text(d)

    async def closed(self, timeout=5):
        try:
            while True:
                await asyncio.wait_for(P.read_frame(self.reader), timeout)
        except asyncio.IncompleteReadError:
            return True
        except (ConnectionError, OSError):
            return True

    def close(self):
        self.writer.close()


class ServerTest(unittest.IsolatedAsyncioTestCase):
    extra = {}

    async def asyncSetUp(self):
        self.tmp = tempfile.mkdtemp()
        cfg = S.load_config()
        cfg.update({'game_port': 0, 'web_port': 0, 'data_dir': self.tmp,
                    'listen': '127.0.0.1'})
        cfg.update(self.extra)
        self.cfg = cfg
        self.relay, self.game, self.page = await S.start(cfg)
        self.port = self.game.sockets[0].getsockname()[1]
        self.web_port = self.page.sockets[0].getsockname()[1]

    async def asyncTearDown(self):
        self.game.close()
        self.page.close()
        await asyncio.sleep(0.05)
        shutil.rmtree(self.tmp, ignore_errors=True)

    async def amiga(self):
        return await Amiga(self.port).open()

    async def make_game(self, player='ANNA', name='FOREST', version=VERSION):
        a = await self.amiga()
        answer = await a.relay('CREATE 1 %s %s\t%s' % (version, player, name))
        self.assertTrue(answer.startswith('CREATED '), answer)
        return a, answer.split()[1]


class Sessions(ServerTest):
    async def test_pair_and_pass_frames(self):
        a, code = await self.make_game()
        b = await self.amiga()
        self.assertEqual(await b.relay('JOIN 1 %s %s BOB' % (VERSION, code)),
                         'PAIRED ANNA')
        t, d = await a.recv()
        self.assertEqual((t, P.text(d)), (P.F_RELAY, 'PAIRED BOB'))
        # HELLO and game frames both ways, unchanged
        a.send(P.F_HELLO, b'\x010.131\x00ANNA\x00')
        self.assertEqual(await b.recv(), (P.F_HELLO, b'\x010.131\x00ANNA\x00'))
        b.send(P.GAME_BASE + 2, b'\x00\x00\x00\x07\x00')
        self.assertEqual(await a.recv(), (P.GAME_BASE + 2, b'\x00\x00\x00\x07\x00'))
        big = bytes(range(250))
        a.send(P.GAME_BASE + 5, big)
        self.assertEqual(await b.recv(), (P.GAME_BASE + 5, big))
        # ping of A answered by B: the server measures B's round trip
        ping = struct.pack('>I', 1234)
        a.send(P.F_PING, ping)
        self.assertEqual(await b.recv(), (P.F_PING, ping))
        await asyncio.sleep(0.03)
        b.send(P.F_PONG, ping)
        self.assertEqual(await a.recv(), (P.F_PONG, ping))
        await asyncio.sleep(0.05)
        st = self.relay.status()
        self.assertEqual(len(st['sessions']), 1)
        g = st['sessions'][0]['guest']
        self.assertIsNotNone(g['rtt_ms'])
        self.assertGreaterEqual(g['rtt_ms'], 20)
        # the end: one side leaves, the other is told and closed
        a.send(P.GAME_BASE + 3, b'purpose')
        await b.recv()
        a.close()
        t, d = await b.recv()
        self.assertEqual(t, P.F_BYE)
        self.assertTrue(await b.closed())
        await asyncio.sleep(0.1)
        self.assertEqual(self.relay.status()['sessions'], [])
        with open(os.path.join(self.tmp, 'sessions.jsonl')) as f:
            rec = json.loads(f.readlines()[-1])
        self.assertEqual(rec['end'], 'game ended')
        self.assertEqual(rec['host'], 'ANNA')
        self.assertEqual(rec['guest'], 'BOB')
        self.assertEqual(self.relay.totals['games'], 1)

    async def test_list(self):
        a1, c1 = await self.make_game('ANNA', 'FOREST')
        a2, c2 = await self.make_game('CARL', 'HILLS')
        b = await self.amiga()
        b.send(P.F_RELAY, 'LIST 1 ' + VERSION)
        lines = []
        while True:
            t, d = await b.recv()
            lines.append(P.text(d))
            if lines[-1] == 'END':
                break
        self.assertEqual(len(lines), 3)
        self.assertTrue(any(l.startswith('SESSION %s ' % c1) and
                            l.endswith('\tANNA\tFOREST\t0') for l in lines))
        self.assertTrue(await b.closed())

    async def test_wrong_code_and_version(self):
        a, code = await self.make_game()
        b = await self.amiga()
        self.assertEqual(await b.relay('JOIN 1 %s ZZZZ BOB' % VERSION),
                         'ERR No such game.')
        c = await self.amiga()
        self.assertEqual(await c.relay('JOIN 1 0.120 %s BOB' % code),
                         'ERR The game needs version %s.' % VERSION)
        d = await self.amiga()
        self.assertEqual(await d.relay('JOIN 2 %s %s BOB' % (VERSION, code)),
                         'ERR Different version.')

    async def test_password(self):
        a = await self.amiga()
        answer = await a.relay('CREATE 1 %s ANNA\tSECRET GAME\tabc12' % VERSION)
        code = answer.split()[1]
        lister = await self.amiga()
        lister.send(P.F_RELAY, 'LIST 1 ' + VERSION)
        t, d = await lister.recv()
        self.assertTrue(P.text(d).endswith('\tSECRET GAME\t1'))
        b = await self.amiga()
        self.assertEqual(await b.relay('JOIN 1 %s %s BOB' % (VERSION, code)),
                         'ERR Password needed.')
        c = await self.amiga()
        self.assertEqual(await c.relay('JOIN 1 %s %s BOB\twrong' % (VERSION, code)),
                         'ERR Wrong password.')
        d2 = await self.amiga()
        self.assertEqual(await d2.relay('JOIN 1 %s %s BOB\tabc12' % (VERSION, code)),
                         'PAIRED ANNA')

    async def test_direct_hello_refused(self):
        a = await self.amiga()
        a.send(P.F_HELLO, b'\x010.131\x00X\x00')
        t, d = await a.recv()
        self.assertEqual(t, P.F_BYE)
        self.assertTrue(await a.closed())

    async def test_maker_leaves(self):
        a, code = await self.make_game()
        a.close()
        await asyncio.sleep(0.1)
        self.assertEqual(self.relay.status()['sessions'], [])

    async def test_bad_first_frame(self):
        a = await self.amiga()
        a.writer.write(b'\x00')
        self.assertTrue(await a.closed())


class Limits(ServerTest):
    extra = {'max_sessions': 2, 'max_per_address': 3}

    async def test_full_and_per_address(self):
        keep = [await self.make_game(), await self.make_game()]
        c = await self.amiga()
        self.assertEqual(await c.relay('CREATE 1 %s X\tY' % VERSION), 'ERR Server full.')
        await asyncio.sleep(0.05)
        d = await self.amiga()    # the 4th connection of 127.0.0.1
        d2 = await self.amiga()
        # 2 games open + d + d2 = 4 > 3: d2 refused at once
        t, data = await d2.recv()
        self.assertEqual(P.text(data), 'ERR Too many connections.')


class Reports(ServerTest):
    async def test_report(self):
        a = await self.amiga()
        a.send(P.F_RELAY, 'REPORT 1 %s F10' % VERSION)
        a.send(P.F_RELAY, 'FIELD email anna@example.com')
        a.send(P.F_RELAY, 'FIELD text The serf stopped.')
        content = bytes(range(256)) * 10
        a.send(P.F_RELAY, 'FILE Settlers-state.save %d' % len(content))
        for i in range(0, len(content), 240):
            a.send(P.F_RELAY_DATA, content[i:i + 240])
        a.send(P.F_RELAY, 'FILE Settlers-state.txt 5')
        a.send(P.F_RELAY_DATA, b'hello')
        a.send(P.F_RELAY, 'DONE')
        t, d = await a.recv()
        self.assertEqual(P.text(d), 'REPORTED 1')
        folders = os.listdir(os.path.join(self.tmp, 'reports'))
        self.assertEqual(len(folders), 1)
        path = os.path.join(self.tmp, 'reports', folders[0])
        with open(os.path.join(path, 'report.json')) as f:
            rec = json.load(f)
        self.assertEqual(rec['email'], 'anna@example.com')
        self.assertEqual(rec['text'], 'The serf stopped.')
        self.assertEqual(rec['version'], VERSION)
        with open(os.path.join(path, 'Settlers-state.save'), 'rb') as f:
            self.assertEqual(f.read(), content)

    async def test_bad_file_name(self):
        a = await self.amiga()
        a.send(P.F_RELAY, 'REPORT 1 %s F10' % VERSION)
        a.send(P.F_RELAY, 'FILE ../evil 3')
        t, d = await a.recv()
        self.assertEqual(P.text(d), 'ERR Bad file.')


class Page(ServerTest):
    async def get(self, path, method='GET'):
        r, w = await asyncio.open_connection('127.0.0.1', self.web_port)
        w.write(('%s %s HTTP/1.0\r\n\r\n' % (method, path)).encode())
        data = await r.read()
        w.close()
        head, _, body = data.partition(b'\r\n\r\n')
        return head.split(b' ')[1], body

    async def test_page_and_json(self):
        a, code = await self.make_game()
        status, body = await self.get('/')
        self.assertEqual(status, b'200')
        self.assertIn(code.encode(), body)
        status, body = await self.get('/stats.json')
        self.assertEqual(json.loads(body)['sessions'][0]['code'], code)
        status, body = await self.get('/nothing')
        self.assertEqual(status, b'404')

    async def test_report_download(self):
        a = await self.amiga()
        a.send(P.F_RELAY, 'REPORT 1 %s F10' % VERSION)
        long_text = 'word ' * 200          # 1000 letters, in pieces
        for i in range(0, len(long_text), 200):
            a.send(P.F_RELAY, 'FIELD text ' + long_text[i:i + 200])
        a.send(P.F_RELAY, 'FILE Settlers-state.save 4')
        a.send(P.F_RELAY_DATA, b'SAVE')
        a.send(P.F_RELAY, 'DONE')
        await a.recv()
        folder = os.listdir(os.path.join(self.tmp, 'reports'))[0]
        status, body = await self.get('/')
        self.assertIn(('/reports/%s.zip' % folder).encode(), body)
        status, body = await self.get('/reports/%s/' % folder)
        self.assertEqual(status, b'200')
        self.assertIn(long_text.strip().encode(), body)
        for path in ('/reports/%s.zip' % folder, '/reports/all.zip'):
            status, body = await self.get(path)
            self.assertEqual(status, b'200')
            import io
            import zipfile
            with zipfile.ZipFile(io.BytesIO(body)) as z:
                self.assertEqual(z.read(folder + '/Settlers-state.save'), b'SAVE')
                rec = json.loads(z.read(folder + '/report.json'))
                self.assertEqual(rec['text'], long_text)
        status, body = await self.get('/reports/%s/Settlers-state.save' % folder)
        self.assertEqual(body, b'SAVE')
        status, _ = await self.get('/reports/%s/../config.json' % folder)
        self.assertEqual(status, b'404')
        status, _ = await self.get('/reports/../config.json')
        self.assertEqual(status, b'404')
        # Delete: off the page, kept in data/deleted/
        status, _ = await self.get('/reports/%s/delete' % folder, 'POST')
        self.assertEqual(status, b'303')
        self.assertEqual(os.listdir(os.path.join(self.tmp, 'reports')), [])
        self.assertTrue(os.path.isdir(os.path.join(self.tmp, 'deleted', folder)))
        status, _ = await self.get('/reports/%s/delete' % folder, 'POST')
        self.assertEqual(status, b'404')
        status, _ = await self.get('/reports/..%2Fconfig.json/delete', 'POST')
        self.assertEqual(status, b'404')


DUNE = 'OpenDUNE-0.52'


class ThreePlayers(ServerTest):
    """A game of 3 (OpenDUNE): the maker has a connection for each player
    that joins (ATTACH)."""
    extra = {'max_per_address': 10}     # all the fake Amigas are 127.0.0.1

    async def make3(self, password=''):
        a1 = await self.amiga()
        answer = await a1.relay('CREATE 1 %s ANNA\tDUNES\t%s\t3' % (DUNE, password))
        self.assertTrue(answer.startswith('CREATED '), answer)
        code = answer.split()[1]
        a2 = await self.amiga()
        self.assertEqual(await a2.relay('ATTACH 1 %s %s' % (DUNE, code)), 'ATTACHED')
        return a1, a2, code

    async def listing(self, version):
        b = await self.amiga()
        b.send(P.F_RELAY, 'LIST 1 ' + version)
        lines = []
        while True:
            t, d = await b.recv()
            if P.text(d) == 'END':
                return lines
            lines.append(P.text(d))

    async def test_three_players(self):
        a1, a2, code = await self.make3()
        lines = await self.listing(DUNE)
        self.assertEqual(len(lines), 1)
        self.assertTrue(lines[0].endswith('\tANNA\tDUNES\t0\t1/3'), lines[0])
        b = await self.amiga()
        self.assertEqual(await b.relay('JOIN 1 %s %s BOB' % (DUNE, code)),
                         'PAIRED ANNA')
        self.assertEqual(P.text((await a1.recv())[1]), 'PAIRED BOB')
        # one place left: still on the list
        lines = await self.listing(DUNE)
        self.assertTrue(lines[0].endswith('\t2/3'), lines)
        c = await self.amiga()
        self.assertEqual(await c.relay('JOIN 1 %s %s CARL' % (DUNE, code)),
                         'PAIRED ANNA')
        self.assertEqual(P.text((await a2.recv())[1]), 'PAIRED CARL')
        self.assertEqual(await self.listing(DUNE), [])
        d = await self.amiga()
        self.assertEqual(await d.relay('JOIN 1 %s %s DAN' % (DUNE, code)),
                         'ERR No such game.')
        # each pair apart: what the maker sends on a connection goes to
        # that player only, and back
        a1.send(P.GAME_BASE + 4, b'to bob')
        a2.send(P.GAME_BASE + 4, b'to carl')
        self.assertEqual(await b.recv(), (P.GAME_BASE + 4, b'to bob'))
        self.assertEqual(await c.recv(), (P.GAME_BASE + 4, b'to carl'))
        c.send(P.GAME_BASE + 5, b'from carl')
        b.send(P.GAME_BASE + 5, b'from bob')
        self.assertEqual(await a2.recv(), (P.GAME_BASE + 5, b'from carl'))
        self.assertEqual(await a1.recv(), (P.GAME_BASE + 5, b'from bob'))
        st = self.relay.status()['sessions'][0]
        self.assertEqual((st['players'], st['max_players'], st['game']),
                         (3, 3, 'dune'))
        self.assertEqual(st['guest2']['player'], 'CARL')
        # Settlers' quit frame (19) is OpenDUNE's START: not the end
        a1.send(P.GAME_BASE + 3, b'start')
        await b.recv()
        # one player leaves: the game goes on for the other two
        c.close()
        t, d = await a2.recv()
        self.assertEqual(t, P.F_BYE)
        self.assertTrue(await a2.closed())
        await asyncio.sleep(0.1)
        self.assertEqual(len(self.relay.status()['sessions']), 1)
        a1.send(P.GAME_BASE + 4, b'still')
        self.assertEqual(await b.recv(), (P.GAME_BASE + 4, b'still'))
        # the maker ends the game (MT_QUIT, 16 + 7)
        a1.send(P.GAME_BASE + 7, b'')
        await b.recv()
        a1.close()
        self.assertTrue(await b.closed())
        await asyncio.sleep(0.1)
        self.assertEqual(self.relay.status()['sessions'], [])
        with open(os.path.join(self.tmp, 'sessions.jsonl')) as f:
            lines = f.readlines()
        self.assertEqual(len(lines), 1)
        rec = json.loads(lines[0])
        self.assertEqual((rec['host'], rec['guest'], rec['guest2'], rec['end']),
                         ('ANNA', 'BOB', 'CARL', 'game ended'))
        self.assertEqual(self.relay.games['dune']['totals']['games'], 1)
        self.assertEqual(self.relay.games['settlers']['totals']['games'], 0)

    async def test_start_with_two_of_three(self):
        a1, a2, code = await self.make3()
        b = await self.amiga()
        self.assertEqual(await b.relay('JOIN 1 %s %s BOB' % (DUNE, code)),
                         'PAIRED ANNA')
        await a1.recv()
        a2.close()                      # the game starts: no third player
        await asyncio.sleep(0.1)
        self.assertEqual(await self.listing(DUNE), [])
        c = await self.amiga()
        self.assertEqual(await c.relay('JOIN 1 %s %s CARL' % (DUNE, code)),
                         'ERR No such game.')
        a1.send(P.GAME_BASE + 4, b'turn')
        self.assertEqual(await b.recv(), (P.GAME_BASE + 4, b'turn'))
        self.assertEqual(len(self.relay.status()['sessions']), 1)

    async def test_attach_again_after_a_player_left(self):
        a1, a2, code = await self.make3()
        b = await self.amiga()
        self.assertEqual(await b.relay('JOIN 1 %s %s BOB' % (DUNE, code)),
                         'PAIRED ANNA')
        await a1.recv()
        b.close()                       # leaves in the lobby
        t, d = await a1.recv()
        self.assertEqual(t, P.F_BYE)
        self.assertTrue(await a1.closed())
        await asyncio.sleep(0.1)
        # the game is still there (the second connection waits); the maker
        # opens a new one in place of the one that went
        a3 = await self.amiga()
        self.assertEqual(await a3.relay('ATTACH 1 %s %s' % (DUNE, code)), 'ATTACHED')
        for name, conn in (('CARL', a2), ('DAN', a3)):
            c = await self.amiga()
            self.assertEqual(await c.relay('JOIN 1 %s %s %s' % (DUNE, code, name)),
                             'PAIRED ANNA')
            self.assertEqual(P.text((await conn.recv())[1]), 'PAIRED ' + name)
        self.assertEqual(await self.listing(DUNE), [])

    async def test_attach_refused(self):
        a = await self.amiga()
        self.assertEqual(await a.relay('ATTACH 1 %s ZZZZ' % DUNE), 'ERR No such game.')
        # a game of 2 takes no second connection
        m, code = await self.make_game(version=DUNE)
        b = await self.amiga()
        self.assertEqual(await b.relay('ATTACH 1 %s %s' % (DUNE, code)),
                         'ERR No such game.')
        # a game of 3 takes one, not two; and only of its version
        a1, a2, code3 = await self.make3()
        c = await self.amiga()
        self.assertEqual(await c.relay('ATTACH 1 %s %s' % (DUNE, code3)),
                         'ERR No such game.')
        d = await self.amiga()
        self.assertEqual(await d.relay('ATTACH 1 %s %s' % (VERSION, code3)),
                         'ERR No such game.')

    async def test_maker_leaves_game_of_three(self):
        a1, a2, code = await self.make3()
        a1.close()
        self.assertTrue(await a2.closed())
        await asyncio.sleep(0.1)
        self.assertEqual(self.relay.status()['sessions'], [])

    async def test_games_apart(self):
        s, scode = await self.make_game('SARA', 'FOREST')
        a1, a2, dcode = await self.make3('pw1')
        lines = await self.listing(VERSION)
        self.assertEqual(len(lines), 1)
        self.assertTrue(lines[0].endswith('\tSARA\tFOREST\t0'), lines[0])
        lines = await self.listing(DUNE)
        self.assertEqual(len(lines), 1)
        self.assertTrue(lines[0].endswith('\tANNA\tDUNES\t1\t1/3'), lines[0])
        b = await self.amiga()
        self.assertEqual(await b.relay('JOIN 1 %s %s BOB' % (VERSION, dcode)),
                         'ERR The game needs version %s.' % DUNE)


class GamePages(Page):
    async def report(self, version, text):
        a = await self.amiga()
        a.send(P.F_RELAY, 'REPORT 1 %s F10' % version)
        a.send(P.F_RELAY, 'FIELD text ' + text)
        for i in range(6):              # 6 files: a game of 3 out of step
            a.send(P.F_RELAY, 'FILE state%d.txt 2' % i)
            a.send(P.F_RELAY_DATA, b'ok')
        a.send(P.F_RELAY, 'DONE')
        t, d = await a.recv()
        self.assertTrue(P.text(d).startswith('REPORTED '), d)

    async def test_subpages(self):
        s, scode = await self.make_game('SARA', 'FOREST')
        d, dcode = await self.make_game('DAVE', 'DUNES', DUNE)
        await self.report(VERSION, 'serf report')
        await self.report(DUNE, 'harvester report')
        status, body = await self.get('/')
        self.assertEqual(status, b'200')
        for word in (scode, dcode, 'serf report', 'harvester report',
                     'href="/settlers"', 'href="/dune"'):
            self.assertIn(word.encode(), body)
        status, body = await self.get('/settlers')
        self.assertEqual(status, b'200')
        self.assertIn(b'Settlers', body)
        self.assertIn(scode.encode(), body)
        self.assertIn(b'serf report', body)
        self.assertNotIn(('>%s<' % dcode).encode(), body)
        self.assertNotIn(b'harvester report', body)
        status, body = await self.get('/dune')
        self.assertEqual(status, b'200')
        self.assertIn(b'OpenDUNE', body)
        self.assertIn(dcode.encode(), body)
        self.assertIn(b'harvester report', body)
        self.assertNotIn(('>%s<' % scode).encode(), body)
        self.assertNotIn(b'serf report', body)
        self.assertEqual(self.reports_count('dune'), 1)

    def reports_count(self, game):
        return self.relay.reports.count(game)


@unittest.skipUnless(os.environ.get('LOAD'), 'LOAD=1 for the load test')
class Load(ServerTest):
    extra = {'max_per_address': 200, 'max_connections': 200}

    async def test_50_games(self):
        games = 50
        seconds = float(os.environ.get('LOAD_S', '20'))
        pairs = []
        for i in range(games):
            a, code = await self.make_game('A%d' % i, 'G%d' % i)
            b = await self.amiga()
            self.assertTrue((await b.relay('JOIN 1 %s %s B%d' % (VERSION, code, i)))
                            .startswith('PAIRED'))
            await a.recv()
            pairs.append((a, b))
        sent = [0]
        got = [0]

        async def talk(src, dst):
            # 0.7 KB/s: a turn of ~56 bytes every 80 ms, a ping a second
            n = 0
            end = time.time() + seconds
            while time.time() < end:
                src.send(P.GAME_BASE + 2, bytes(54))
                sent[0] += 1
                if n % 12 == 0:
                    src.send(P.F_PING, struct.pack('>I', n))
                n += 1
                await asyncio.sleep(0.08)

        async def listen(dst):
            try:
                while True:
                    t, d = await dst.recv(timeout=seconds + 5)
                    if t == P.GAME_BASE + 2:
                        got[0] += 1
                    elif t == P.F_PING:
                        dst.send(P.F_PONG, d)
            except (asyncio.TimeoutError, asyncio.IncompleteReadError):
                pass

        t0 = time.time()
        tasks = []
        for a, b in pairs:
            tasks += [talk(a, b), talk(b, a), listen(a), listen(b)]
        await asyncio.wait([asyncio.ensure_future(t) for t in tasks],
                           timeout=seconds + 8)
        print('\n%d games, %.0f s: %d turns sent, %d passed on'
              % (games, time.time() - t0, sent[0], got[0]))
        self.assertGreater(got[0], sent[0] * 0.99)


if __name__ == '__main__':
    unittest.main()
